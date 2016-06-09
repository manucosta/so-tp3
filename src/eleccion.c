#include <stdio.h>
#include <mpi.h>
#include "eleccion.h"

static t_pid siguiente_pid(t_pid pid, int es_ultimo){
 t_pid res= 0; /* Para silenciar el warning del compilador. */

 if (es_ultimo)
	res= 1;
 else
	res= pid+1;

 return res;
}

void iniciar_eleccion(t_pid pid, int es_ultimo) {
/*
	Si no está bien documentado, no aprueba.
*/
	int buffer[] = {pid, pid};
	t_pid siguiente = siguiente_pid(pid, es_ultimo);
	MPI_Request request;
	//bool ack = false;
	//while{ack}{
	MPI_Isend(&buffer, 2, MPI_INT, siguiente, TAG_ELECCION, MPI_COMM_WORLD, &request);
	//}
}

void eleccion_lider(t_pid pid, int es_ultimo, unsigned int timeout){
	static t_status status= NO_LIDER;
	double ahora= MPI_Wtime();
	double tiempo_maximo= ahora+timeout;
	t_pid proximo= siguiente_pid(pid, es_ultimo);

 	while (ahora<tiempo_maximo) {
	 /*
	  Si no está bien documentado, no aprueba.
   */
	  int flag_msg = 0;
	  MPI_Status estado;
	  while(!flag_msg) {
	  	MPI_Iprobe(MPI_ANY_SOURCE, TAG_ELECCION, MPI_COMM_WORLD, &flag_msg, &estado);
	  	ahora = MPI_Wtime();
	  	if(ahora >= tiempo_maximo) {
	  		break;
	  	}
	  }
	  int buffer[2];
	  MPI_Request request;
	  MPI_Irecv(&buffer, 2, MPI_INT,  estado.MPI_SOURCE, TAG_ELECCION, MPI_COMM_WORLD, &request);
	  int i = buffer[0];
	  int cl = buffer[1];
	  if(i == pid) { //Dio toda la vuelta
	  	if(cl > pid) { //El líder está adelante
	  		buffer[0] = cl; //El par es <cl,cl>
	  	} else { //No dio toda la vuelta
	  		status = LIDER;
	  		continue;
	  	}
	  } else {
	  	if(pid > cl) {
	  		buffer[1] = pid;
	  	}
	  }
	  MPI_Isend(&buffer, 2, MPI_INT, proximo, TAG_ELECCION, MPI_COMM_WORLD, &request);
	 	/* Actualizo valor de la hora. */
		ahora= MPI_Wtime();
	}

 /* Reporto mi status al final de la ronda. */
 printf("Proceso %u %s líder.\n", pid, (status==LIDER ? "es" : "no es"));
}
