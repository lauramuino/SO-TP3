#include "srv.h"




void servidor(int mi_cliente)
{
    MPI_Status status; int origen, tag;
    int hay_pedido_local = FALSE;
    int listo_para_salir = FALSE;
    int mi_rank = mi_cliente -1; //el cliente tiene identificador -1 a su servidor
    int server; // iterador para los fors
    
    while( ! listo_para_salir ) {
        
        MPI_Recv(NULL, 0, MPI_INT, ANY_SOURCE, ANY_TAG, COMM_WORLD, &status);
        origen = status.MPI_SOURCE;
        tag = status.MPI_TAG;
        
        if (tag == TAG_PEDIDO) {
            if(origen == mi_cliente){
                //pedido de mi cliente
                debug("Mi cliente solicita acceso exclusivo");
                assert(hay_pedido_local == FALSE);
                hay_pedido_local = TRUE;
                
                debug("Obteniendo permiso de los demás servers");
                for (server = 0; server < cant_ranks/2; server++){
                    //veo no ser yo
                    if(server != mi_rank){
                        MPI_Ssend(NULL, 0, MPI_INT, 2*server, TAG_PEDIDO, COMM_WORLD);
                    }
                }

                for (server = 0; server < cant_ranks/2; server++){
                    ///NO SE SI FUNCIONA ESTA PARTE ESPECIALMENTE
                    // no tengo idea de los demas servers, ni verificar que los recibi de todos y cada uno del resto de los servidores
                    //veo no ser yo
                    if(server != mi_rank){
                        MPI_Recv(NULL, 0, MPI_INT, server, TAG_OTORGADO, COMM_WORLD, &status);
                    }
                }
                MPI_Send(NULL, 0, MPI_INT, mi_cliente, TAG_OTORGADO, COMM_WORLD);

            }else{
                //estan pidiendo permiso para que haya exclusion mutua
                if(!hay_pedido_local){
                    //le doy permiso, pues mi cliente no esta en la seccion crítica
                    debug("Dándole permiso a algun server");
                    MPI_Send(NULL, 0, MPI_INT, TAG_OTORGADO, COMM_WORLD, &status);
                }else{

                }
            }
        }
        
        else if (tag == TAG_LIBERO) {
            assert(origen == mi_cliente);
            debug("Mi cliente libera su acceso exclusivo");
            assert(hay_pedido_local == TRUE);
            hay_pedido_local = FALSE;
        }
        
        else if (tag == TAG_TERMINE) {
            assert(origen == mi_cliente);
            debug("Mi cliente avisa que terminó");
            listo_para_salir = TRUE;
        }
        
    }
    
}

