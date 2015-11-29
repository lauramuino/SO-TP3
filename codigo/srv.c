#include "srv.h"
#include <string.h> // Para memcpy

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
    int servers_que_otorgaron[cant_ranks/2];
    int acumulador = 0; // Solo para mensajes de debug
    int reloj = 0;
    int our_number = 0;
    int buffer; // Buffer del mensaje mpi - utilizado para enviar/recibir numero de secuencia (reloj)
    int servers_muertos = 0; // Solo para mensajes de debug
    char debug_msg[255]; // para utilizar cadenas con formato para la funcion debug

    int iterador; // Inicializando pendientes para RESPUESTAS DEFERIDAS
    for(iterador = 0; iterador < cant_ranks/2; iterador++){
        pendientes[iterador] = 0;
        a_servers_muertos[iterador] = 0; // Inicializando arreglo de servers muertos
        servers_que_otorgaron[iterador] = 0;
    }
    servers_que_otorgaron[mi_rank/2] = 1; //siempre me doy permiso a mi mismo

    
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
                            //MPI_Ssend(&buffer, 1, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD); // Provoca DEADLOCK 
                            MPI_Send(&buffer, 1, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD); // Soluciona el HOLD & WAIT
                        }
                    }
                    debug("Marcando que hay pedido local = TRUE");
                    hay_pedido_local = TRUE;
                }

            }else{
                //me pide permiso un server
               
                if( buffer > reloj){ 
                    sprintf(debug_msg, "Se actualizo el reloj del server %i - valor antiguo: %i - nuevo valor: %i", mi_rank, reloj, buffer);
                    debug(debug_msg);
                    reloj = buffer; //actualizo mi reloj de ser necesario
                }
              
                if(cliente_en_zona_critica){
                    sprintf(debug_msg, "Tengo cliente en zona crítica - dejando pedido de server rank %i pendiente", origen);
                    debug(debug_msg);
                    pendientes[origen/2] = 1;
                } else {
                    if(!hay_pedido_local){
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
                
                // aviso que me voy 
                for (iterador = 0; iterador < cant_ranks/2; iterador++){
                    if( (iterador*2 != mi_rank) && (a_servers_muertos[iterador] == 0) ){
						//evito enviar el mensaje a los servers que ya estan muertos
                        sprintf(debug_msg, "Avisando que TERMINE a server rank %i", iterador *2);
                        debug(debug_msg);
                        //MPI_Ssend(NULL, 0, MPI_INT, iterador*2, TAG_TERMINE, COMM_WORLD); cambiado por no bloqueante para evitar deadlock
                        MPI_Send(NULL, 0, MPI_INT, iterador*2, TAG_TERMINE, COMM_WORLD);
                    }
                }
                // NO OTORGO EXPLICITAMENTE LOS PERMISOS PENDIENTES - VER MAS ABAJO EN LOS SERVERS QUE RECIBEN TAG_TERMINE
            } else { // Me llegó de otro server
                assert(origen % 2 == 0); // Es un server
                servers_muertos++;
                a_servers_muertos[origen/2] = 1; // Lo marco como muerto
                pendientes[origen/2] = 0;
                sprintf(debug_msg, "Se murió el server rank %i - Cantidad de servers muertos %i", origen, servers_muertos);
                debug(debug_msg);
                // No me enviaron TAG_OTORGADO al morir, por eso:
                // ACA MANEJO LOS PERMISOS PENDIENTES
                if(hay_pedido_local){
                    assert(!cliente_en_zona_critica);
                    int puedo_entrar = TRUE;
                    for (iterador = 0; iterador < cant_ranks/2; iterador ++){
						//si encuentro un server que no otorgo, me fijo que no este muerto
                        if ((servers_que_otorgaron[iterador] == 0) && (a_servers_muertos[iterador] == 0)){
                            puedo_entrar = FALSE;
                            break;
                        }
                    } 
                    if (puedo_entrar){
                        sprintf(debug_msg, "Mi cliente entra a zona critica - acumulado = %i - muertos = %i", acumulador, servers_muertos);
                        debug(debug_msg);
                        cliente_en_zona_critica = TRUE;
                        MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
                        hay_pedido_local = FALSE;
                        acumulador = 0;
                        memcpy(servers_que_otorgaron, a_servers_muertos, (cant_ranks/2) * sizeof(int));
                        servers_que_otorgaron[mi_rank/2] = 1;
                    } else {
                        servers_que_otorgaron[origen/2] = 1;
                    }
                } 
            }
        }

        else if(tag == TAG_OTORGADO){
            //un server me dio permiso
            sprintf(debug_msg, "Me otorgo permiso el server rank %i", origen);
            debug(debug_msg);
            acumulador++;
            servers_que_otorgaron[origen/2] = 1;
            int puedo_entrar = TRUE;
            for (iterador = 0; iterador < cant_ranks/2; iterador ++){
				//si encuentro un server que no otorgo, me fijo que no este muerto
                if ((servers_que_otorgaron[iterador] == 0) && (a_servers_muertos[iterador] == 0)){
                    puedo_entrar = FALSE;
                    break;
                }
            }
            
            if (puedo_entrar){
                sprintf(debug_msg, "Mi cliente entra a zona critica - acumulado = %i - muertos = %i", acumulador, servers_muertos);
                debug(debug_msg);
                cliente_en_zona_critica = TRUE;
                //si todos me dieron permiso le otorgo la zona critica a mi cliente
             
                MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
                //el pedido fue cumplido, asi q.. ya no hay pedido local
                hay_pedido_local = FALSE;
                acumulador = 0; // Reseteo acumulador
                // Reseteo servers_que_otorgaron incluyendo a los server que murieron
                memcpy(servers_que_otorgaron, a_servers_muertos, (cant_ranks/2) * sizeof(int));
                servers_que_otorgaron[mi_rank/2] = 1;
            }
        }
    }
    
}

