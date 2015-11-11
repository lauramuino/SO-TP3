#include "srv.h"

int comparacion(int rank_i, int rank_j){
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
    int acumulador = 0;
    int reloj = 0;
    int our_number = 0;
    int buffer;
    int servers_muertos = 0;

    int iterador;
    for(iterador = 0; iterador < cant_ranks/2; iterador++){
        pendientes[iterador] = 0;
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
                debug("Mi cliente solicita acceso exclusivo");
                assert(hay_pedido_local == FALSE);
                

                buffer = our_number;

                debug("Obteniendo permiso de los demás servers");
                if(cant_ranks == 2){
                    MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
                    cliente_en_zona_critica = TRUE;
                }else{

                    for (server = 0; server < cant_ranks/2; server++){
                        //veo no ser yo
                        if(server != mi_rank){
                            MPI_Ssend(&buffer, 1, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD);
                        }
                    }
                    hay_pedido_local = TRUE;
                }

            }else{
                //me pide permiso un server
                //int otro_reloj = buffer;
                if( buffer > reloj) reloj = buffer; //actualizo mi reloj de ser necesario

                if(cliente_en_zona_critica){
                    pendientes[origen/2] = 1;
                }else{

                    if(!hay_pedido_local){
                        debug("Dándole permiso a algun server");
                        MPI_Send(NULL, 0, MPI_INT, origen,TAG_OTORGADO, COMM_WORLD);
                    }else{
                        //hago las comparaciones de mierda
                        if(buffer == our_number){
                            if(comparacion(mi_rank, origen)){
                                pendientes[origen/2] = 1;
                            }else{
                                MPI_Send(NULL, 0, MPI_INT, origen,TAG_OTORGADO, COMM_WORLD);
                            }
                        }else{
                            if(buffer < our_number){
                                MPI_Send(NULL, 0, MPI_INT, origen,TAG_OTORGADO, COMM_WORLD);
                            }else{
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

            //otorgo a mis pendientes el permiso
            for (iterador = 0; iterador < cant_ranks/2; iterador++){
                if( (pendientes[iterador] == 1) && (iterador*2 != mi_rank) ){
                    pendientes[iterador] = 0;
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
                    if( (pendientes[iterador] == 1) && (iterador*2 != mi_rank) ){
                        //pendientes[iterador] = 0;
                        MPI_Send(NULL, 0, MPI_INT, iterador*2,TAG_OTORGADO, COMM_WORLD);
                    }
                    debug("concha de tu madre");
                }
                //y aviso que me voy
                for (iterador = 0; iterador < cant_ranks/2; iterador++){
                    if(iterador*2 != mi_rank)
                        MPI_Ssend(NULL, 0, MPI_INT, iterador*2, TAG_TERMINE, COMM_WORLD);
                }
            }else{
                servers_muertos++;
                pendientes[origen] = 0;
                if(hay_pedido_local){
                    
                }
            }
        }

        else if(tag == TAG_OTORGADO){
            //un server me dio permiso
            acumulador++;
            if(acumulador == cant_ranks/2 -1){
                debug("Mi cliente entra a zona critica");
                cliente_en_zona_critica = TRUE;
                //si todos me dieron permiso le otorgo la zona critica a mi cliente
                acumulador = servers_muertos;
                //el pedido fue cumplido, asi q.. ya no hay pedido local
                hay_pedido_local = FALSE;
                MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);
            }
        }
        
    }
    
}

