#include <stdio.h>
#include <mpi.h>
#include "eleccion.h"

const int t = 3;

static t_pid siguiente_pid(t_pid pid, int es_ultimo){
 t_pid res= 0; /* Para silenciar el warning del compilador. */

 if (es_ultimo)
	res= 1;
 else
	res= pid+1;

 return res;
}

void iniciar_eleccion(t_pid pid, int es_ultimo) {
	int buffer[] = {pid, pid};
	t_pid siguiente = siguiente_pid(siguiente, es_ultimo);;
	MPI_Request request;
	
	int ack = 0;
	int flag_msg;
	MPI_Status estado;
	while(ack == 0){
		flag_msg = 0;
		MPI_Isend(&buffer, //Donde est� el mensaje que mandamos
							2, //Cu�ntos datos env�o
							MPI_INT, //Qu� tipo de dato env�o
							siguiente, 
							TAG_ELECCION, 
							MPI_COMM_WORLD, 
							&request
							);
		int tiempo_maximo = MPI_Wtime() + t;
		int ahora = MPI_Wtime();
		while(!flag_msg && ahora < tiempo_maximo) {
	  	MPI_Iprobe(siguiente, 
	  						 TAG_ACK, 
	  						 MPI_COMM_WORLD, 
	  						 &flag_msg, 
	  						 &estado
	  						 );
	  	ahora = MPI_Wtime();
	  }
	  siguiente = siguiente_pid(siguiente, 0);
	  if(ahora >= tiempo_maximo) continue;
	  int coso;
	  MPI_Irecv(&coso, //Donde guardamos el mensaje que llega
	  					1, //Cu�ntos datos me llegan
	  					MPI_DOUBLE,  //Qu� tipo de dato me llega
	  					estado.MPI_SOURCE, 
	  					TAG_ACK, 
	  					MPI_COMM_WORLD, 
	  					&request
	  					);
	  if(coso <= tiempo_maximo) ack = 1;
	}
}

void eleccion_lider(t_pid pid, int es_ultimo, unsigned int timeout){
	static t_status status= NO_LIDER;
	double ahora= MPI_Wtime();
	double tiempo_maximo= ahora+timeout;
	t_pid proximo= siguiente_pid(pid, es_ultimo);
	int flag_msg;

 	while (ahora<tiempo_maximo) {
	  flag_msg = 0;
	  MPI_Status estado;
	  while(!flag_msg) {
	  	MPI_Iprobe(MPI_ANY_SOURCE, //Espero un mensaje de cualquier fuente
	  						 TAG_ELECCION, //
	  						 MPI_COMM_WORLD, //
	  						 &flag_msg, //Si recib� un mensaje se pone en 1
	  						 &estado //Me dice qui�n y con que tag me mand� el mensaje
	  						 );
	  	ahora = MPI_Wtime();
	  	if(ahora >= tiempo_maximo) break;
	  }
	  if(ahora >= tiempo_maximo) break;
	  /***Le aviso al proceso que me mand� la tupla que la recib�***/
	  MPI_Isend(&ahora, //Donde est� el mensaje que mandamos
					1, //Cu�ntos datos env�o
					MPI_DOUBLE, //Qu� tipo de dato env�o
					estado.MPI_SOURCE, //Le respondo a quien me env�o la tupla
					TAG_ACK, 
					MPI_COMM_WORLD, 
					&request
					);
	  
	  int buffer[2];
	  MPI_Request request;
	  MPI_Irecv(&buffer, //Donde guardamos el mensaje que llega
	  					2, //Cu�ntos datos me llegan
	  					MPI_INT,  //Qu� tipo de dato me llega
	  					estado.MPI_SOURCE, 
	  					TAG_ELECCION, 
	  					MPI_COMM_WORLD, 
	  					&request
	  					);
	  int i = buffer[0];
	  int cl = buffer[1];
	  if(i == pid) { //Dio toda la vuelta
	  	if(cl > pid) { //El l�der est� adelante, hay que avisarle
	  		buffer[0] = cl; //El par es <cl,cl>
	  	} else { //El l�der soy yo
	  		status = LIDER;
	  		continue;
	  	}
	  } else { //No dio toda la vuelta
	  	if(pid > cl) { //Nuevo l�der potencial
	  		buffer[1] = pid;
	  	}
	  }
	  MPI_Isend(&buffer, //Donde est� el mensaje que mandamos
	  					2, //Cu�ntos datos env�o
	  					MPI_INT, //Qu� tipo de dato env�o
	  					proximo, //Le mando el mensaje al siguiente del anillo
	  					TAG_ELECCION, //
	  					MPI_COMM_WORLD, //
	  					&request
	  					);
	 	/* Actualizo valor de la hora. */
		ahora= MPI_Wtime();
	}

 /* Reporto mi status al final de la ronda. */
 printf("Proceso %u %s l�der.\n", pid, (status==LIDER ? "es" : "no es"));
}
