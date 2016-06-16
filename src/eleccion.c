#include <stdio.h>
#include <mpi.h>
#include "eleccion.h"

const int t = 1;

static t_pid siguiente_pid(t_pid pid, int es_ultimo) {
 t_pid res= 0; /* Para silenciar el warning del compilador. */

 if (es_ultimo)
  res= 1;
 else
  res= pid+1;

 return res;
}

void iniciar_eleccion(t_pid pid, int es_ultimo) {
  int buffer[] = {pid, pid}; // Creamos la tupla <i, cl> a enviar con i = ID y cl = ID
  t_pid siguiente = siguiente_pid(pid, es_ultimo); // Inicializamos siguiente con el valor = (ID+1) � (1) dependiendo si es el �ltimo proceso en el anillo o no
  MPI_Request request; // Creamos la estructura necesaria para poder invocar a la funci�n MPI_Isend() y MPI_Irecv()
  
  int ack = 0; // ack va a servir como una variable booleana para verificar si le lleg� o no el mensaje
  int flag_msg; // flag_msg va a servir como una variable booleana para verificar si este proceso recibi� el mensaje
  MPI_Status estado; // Variable necesaria para almacenar la informaci�n del mensaje recibido
  while(ack == 0) { // Este while va a estar ciclando mientras no le haya llegado el mensaje al siguiente en el anillo (se le acab� el timeout porque esta muerto, o bien porque estaba ocupado con otra cosa)
    flag_msg = 0; // En cada iteraci�n reinicializamos el flag en 0 (false)
    MPI_Isend(&buffer, 2, MPI_INT,//
              siguiente,          // A qui�n se lo env�o
              TAG_ELECCION,       // La clasificaci�n del mensaje
              MPI_COMM_WORLD,     // El comm por defecto que tiene a todos los procesos del 0 al N-1, con N = total de procesos
              &request            // Identifica la comunicaci�n entre los procesos que intervienen en la misma
              );
    int tiempo_maximo_ack = MPI_Wtime() + t; // Calculo el tiempo m�ximo de espera para esperar el ACK
    int ahora = MPI_Wtime(); // Guardo el tiempo actual para poder comparar y determinar si me pas� del timeout o no
    while(!flag_msg && ahora <= tiempo_maximo_ack) { // Este while va a estar ciclando mientras no se haya recibido un mensaje y no se haya pasado el tiempo m�ximo para el ACK
      MPI_Iprobe(siguiente,       // De qui�n voy a ver si me envio un mensaje
                 TAG_ACK,         // 
                 MPI_COMM_WORLD,  // 
                 &flag_msg,       // 
                 &estado          // 
                 );
      ahora = MPI_Wtime();
    }
    siguiente = siguiente_pid(siguiente, 0);
    if(ahora > tiempo_maximo_ack) continue;
    printf("Soy: %d y recibi ACK de: %d\n", pid, estado.MPI_SOURCE);  
    double respuesta;
    MPI_Irecv(&respuesta, //Donde guardamos el mensaje que llega
              1, //Cu�ntos datos me llegan
              MPI_DOUBLE,  //Qu� tipo de dato me llega
              estado.MPI_SOURCE, 
              TAG_ACK, 
              MPI_COMM_WORLD, 
              &request
              );
    if(respuesta <= tiempo_maximo_ack) ack = 1;
  }
}

void eleccion_lider(t_pid pid, int es_ultimo, unsigned int timeout){
  static t_status status= NO_LIDER;
  double ahora= MPI_Wtime();
  double tiempo_maximo= ahora+timeout;
  t_pid proximo = siguiente_pid(pid, es_ultimo);
  int flag_msg;
  MPI_Request request;

  while (ahora<tiempo_maximo) {
    flag_msg = 0;
    MPI_Status estado;
    while(!flag_msg && ahora < tiempo_maximo) {
      MPI_Iprobe(MPI_ANY_SOURCE, //Espero un mensaje de cualquier fuente
                 TAG_ELECCION, //
                 MPI_COMM_WORLD, //
                 &flag_msg, //Si recib� un mensaje se pone en 1
                 &estado //Me dice qui�n y con que tag me mand� el mensaje
                 );
      ahora = MPI_Wtime();
    }
    if(ahora >= tiempo_maximo) break;
    /***Le aviso al proceso que me mand� la tupla que la recib�***/
    printf("Soy: %d, recibi mensaje de: %d y le respond�.\n", pid, estado.MPI_SOURCE);
    char ok[] = {'O', 'K'};
    MPI_Isend(&ok, //Donde est� el mensaje que mandamos
          2, //Cu�ntos datos env�o
          MPI_CHAR, //Qu� tipo de dato env�o
          estado.MPI_SOURCE, //Le respondo a quien me env�o la tupla
          TAG_ACK, 
          MPI_COMM_WORLD, 
          &request
          );
    
    int buffer[2];
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

    int ack = 0;
    int flag_msg_ack = 0;
    MPI_Status estado_ack;
    proximo = siguiente_pid(pid, es_ultimo);
    while(ack == 0){
      flag_msg_ack = 0;
      MPI_Isend(&buffer, //Donde est� el mensaje que mandamos
                2, //Cu�ntos datos env�o
                MPI_INT, //Qu� tipo de dato env�o
                proximo, 
                TAG_ELECCION, 
                MPI_COMM_WORLD, 
                &request
                );
      printf("Soy: %d y envie a: %d\n", pid, proximo);
      int tiempo_maximo_ack = MPI_Wtime() + t;
      ahora = MPI_Wtime();
      while(!flag_msg_ack && ahora <= tiempo_maximo_ack) {
        MPI_Iprobe(proximo, 
                   TAG_ACK, 
                   MPI_COMM_WORLD, 
                   &flag_msg_ack, 
                   &estado_ack
                   );
        ahora = MPI_Wtime();
      }
      int asd = proximo;//sacar despues
      proximo = siguiente_pid(proximo, 0);//x absurdo
      if(ahora > tiempo_maximo_ack){
        printf("Soy: %d y %d no me respondio. Envio a: %d\n", pid, asd, proximo);
        continue;
      }
      //printf("Soy: %d y recibi mensaje de: %d\n", pid, estado_ack.MPI_SOURCE);
      char respuesta[] = {'K', 'O'};
      MPI_Irecv(&respuesta,               //Donde guardamos el mensaje que llega
                2,                        //Cu�ntos datos me llegan
                MPI_CHAR,               //Qu� tipo de dato me llega
                estado_ack.MPI_SOURCE, 
                TAG_ACK, 
                MPI_COMM_WORLD, 
                &request
                );
      if(respuesta[0] == 'O' || respuesta[1] == 'K') ack = 1;
    }
    ahora= MPI_Wtime();
  }

 /* Reporto mi status al final de la ronda. */
 printf("Proceso %u %s l�der.\n", pid, (status==LIDER ? "es" : "no es"));
}
