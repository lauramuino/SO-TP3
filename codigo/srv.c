#include "srv.h"

int comparacion(int rank_i, int rank_j){
// Si el primero es menor que el segundo -> TRUE
    assert (rank_i != rank_j);
    if(rank_i > rank_j){
        return FALSE;
    }else{
        return TRUE;
    }
}


void servidor(int mi_cliente)
{
    MPI_Status status; int origen, tag;
    int hay_pedido_local = FALSE;
    int listo_para_salir = FALSE;
    int cliente_en_zona_critica = FALSE;
    int mi_rank = mi_cliente -1; //el cliente tiene identificador -1 a su servidor
    int server; // iterador para los fors
    int pendientes[cant_ranks/2];
    int a_servers_muertos[cant_ranks/2];
    int acumulador = 0;
    int reloj = 0;
    int our_number = 0;
    int buffer;
    int servers_muertos = 0;
    char debug_msg[255];

    int iterador; // Inicializando pendientes para RESPUESTAS DEFERIDAS
    for(iterador = 0; iterador < cant_ranks/2; iterador++){
        pendientes[iterador] = 0;
        a_servers_muertos[iterador] = 0; // Inicializando arreglo de servers muertos
    }

    
    while( ! listo_para_salir ) {
        
        MPI_Recv(&buffer, 1, MPI_INT, ANY_SOURCE, ANY_TAG, COMM_WORLD, &status);
        origen = status.MPI_SOURCE;
        tag = status.MPI_TAG;
        
        if (tag == TAG_PEDIDO) {
            if(origen == mi_cliente){
                //pedido de mi cliente
                reloj++;
                our_number = reloj;
                sprintf(debug_msg, "Mi cliente - rank %i - solicita acceso exclusivo", mi_cliente);
                debug(debug_msg);
                assert(hay_pedido_local == FALSE);
                
                buffer = our_number;

                debug("Obteniendo permiso de los demás servers");
                if( (cant_ranks == 2) || (servers_muertos == (cant_ranks/2) -1 ) ){ // No hay otros servers o se murieron todos
                    sprintf(debug_msg, "No existen otros servers o murieron todos");
                    debug(debug_msg);
                    MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
                    cliente_en_zona_critica = TRUE;
                }else{
                    for (server = 0; server < cant_ranks/2; server++){
                        //veo no ser yo y no le envio a los muertos
                        if(((server * 2) != mi_rank) && (a_servers_muertos[server] == 0)){
                            sprintf(debug_msg, "Pidiendo permiso al server rank %i con seq: %i", server*2, our_number);
                            debug(debug_msg);
                            //MPI_Ssend(&buffer, 1, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD); // Provoca DEADLOCK - HOLD & WAIT
                            MPI_Send(&buffer, 1, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD);
                        }
                    }
                    debug("Marcando que hay pedido local = TRUE");
                    hay_pedido_local = TRUE;
                }

            }else{
                //me pide permiso un server
                //int otro_reloj = buffer;
                if( buffer > reloj){ 
                    sprintf(debug_msg, "Se actualizo el reloj del server %i - valor antiguo: %i - nuevo valor: %i", mi_rank, reloj, buffer);
                    debug(debug_msg);
                    reloj = buffer; //actualizo mi reloj de ser necesario
                }
                // NO ME CONVENCE - QUE PASA SI YO YA ENVIE PEDIDO DE MI CLIENTE?
                // CREO QUE NO PASA NADA,YA QUE YA ENVIÉ Y NO ENVIO DE NUEVO

                if(cliente_en_zona_critica){
                    sprintf(debug_msg, "Tengo cliente en zona crítica - dejando pedido de server rank %i pendiente", origen);
                    debug(debug_msg);
                    pendientes[origen/2] = 1;
                } else {
                    if(!hay_pedido_local){
                        //debug("Dándole permiso a algun server");
                        sprintf(debug_msg, "No hay pedido local - Dando permiso al server rank %i con seq: %i", origen , buffer);
                        debug(debug_msg);
                        MPI_Send(NULL, 0, MPI_INT, origen, TAG_OTORGADO, COMM_WORLD);
                    }else{
                        //hago las comparaciones de mierda
                        sprintf(debug_msg, "Hay pedido local - Viendo a quien le toca");
                        debug(debug_msg);
                        if(buffer == our_number){
                            if(comparacion(mi_rank, origen)){
                                // mi_rank < origen:
                                sprintf(debug_msg, "Hay pedido local - DIFIRIENDO server rank %i con seq: %i - MISMO seq que yo", origen, buffer); 
                                debug(debug_msg);
                                pendientes[origen/2] = 1;
                            }else{
                                sprintf(debug_msg, "Hay pedido local - GRANTING server rank %i con seq: %i - Menor RANK que yo", origen, buffer);
                                debug(debug_msg);
                                MPI_Send(NULL, 0, MPI_INT, origen,TAG_OTORGADO, COMM_WORLD);
                            }
                        }else{
                            if(buffer < our_number){
                                sprintf(debug_msg, "Hay pedido local - GRANTING server rank %i con seq: %i - Menor seq que yo", origen, buffer);
                                debug(debug_msg);
                                MPI_Send(NULL, 0, MPI_INT, origen,TAG_OTORGADO, COMM_WORLD);
                            }else{
                                sprintf(debug_msg, "Hay pedido local - DIFIRIENDO server rank %i con seq:  %i - Yo tengo menor seq", origen, buffer);
                                debug(debug_msg);
                                pendientes[origen/2] = 1;
                            }
                        }
                    }
                }
            }
        }
        
        else if (tag == TAG_LIBERO) {
            assert(origen == mi_cliente);
            debug("Mi cliente libera su acceso exclusivo");
            assert(cliente_en_zona_critica  == TRUE);
            cliente_en_zona_critica = FALSE;

            //otorgo a mis pendientes (no muertos) el permiso
            for (iterador = 0; iterador < cant_ranks/2; iterador++){
                if( (pendientes[iterador] == 1) && (iterador*2 != mi_rank) && (a_servers_muertos[iterador] == 0) ){
                    pendientes[iterador] = 0;
                    sprintf(debug_msg, "Otorgando GRANT a server rank %i", iterador *2);
                    debug(debug_msg);
                    MPI_Send(NULL, 0, MPI_INT, iterador*2,TAG_OTORGADO, COMM_WORLD);
                }
            }
        } 
        else if (tag == TAG_TERMINE) {
            
            if(origen == mi_cliente){
                debug("Mi cliente avisa que terminó");
                listo_para_salir = TRUE;
                
                //otorgo a mis pendientes el permiso
                for (iterador = 0; iterador < cant_ranks/2; iterador++){
                    //if( (pendientes[iterador] == 1) && (iterador*2 != mi_rank) ){
                    if( (pendientes[iterador] == 1) && (iterador*2 != mi_rank) && (a_servers_muertos[iterador] == 0) ){
                        //pendientes[iterador] = 0;
                        sprintf(debug_msg, "Otorgando GRANT a server rank %i", iterador *2);
                        debug(debug_msg);
                        MPI_Send(NULL, 0, MPI_INT, iterador*2,TAG_OTORGADO, COMM_WORLD);
                    }
                }
                debug("Le otorgué el GRANT a todos los pendientes");
                //y aviso que me voy
                for (iterador = 0; iterador < cant_ranks/2; iterador++){
                    if( (iterador*2 != mi_rank) && (a_servers_muertos[iterador] == 0) ){
                        sprintf(debug_msg, "Avisando que TERMINE a server rank %i", iterador *2);
                        debug(debug_msg);
                        //MPI_Ssend(NULL, 0, MPI_INT, iterador*2, TAG_TERMINE, COMM_WORLD);
                        MPI_Send(NULL, 0, MPI_INT, iterador*2, TAG_TERMINE, COMM_WORLD);
                    }
                }
                // y me voy - Vuelvo al MAIN
                return;
            }else{ // Me llegó de otro server
                assert(origen % 2 == 0); // Es un server
                servers_muertos++;
                a_servers_muertos[origen/2] = 1; // Lo marco como muerto
                pendientes[origen/2] = 0;
                sprintf(debug_msg, "Se murió el server rank %i - Cantidad de servers muertos %i", origen, servers_muertos);
                debug(debug_msg);
                if(hay_pedido_local){
                    // Hace falta otorgarle agregar el GRANT del origen a esta altura?
                    // Creo que no - Debería haber otorgado el GRANT antes de enviar el TAG_TERMINE
                }
            }
        }

        else if(tag == TAG_OTORGADO){
            //un server me dio permiso
            acumulador++;
            if( (acumulador + servers_muertos ) == cant_ranks/2 -1){
                debug("Mi cliente entra a zona critica");
                cliente_en_zona_critica = TRUE;
                //si todos me dieron permiso le otorgo la zona critica a mi cliente
                // acumulador = servers_muertos;
                //el pedido fue cumplido, asi q.. ya no hay pedido local
                MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
                hay_pedido_local = FALSE;
                acumulador = 0; // Reseteo acumulador
            }
        }
        
    }
    
}

