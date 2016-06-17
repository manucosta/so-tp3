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
  t_pid siguiente = siguiente_pid(pid, es_ultimo); // Inicializamos siguiente con el valor = (ID+1) ó (1) dependiendo si es el último proceso en el anillo o no
  MPI_Request request; // Creamos la estructura necesaria para poder invocar a la función MPI_Isend() y MPI_Irecv()
  
  int ack = 0; // ack va a servir como una variable booleana para verificar si le llegó o no el mensaje
  int flag_msg; // flag_msg va a servir como una variable booleana para verificar si este proceso recibió el mensaje
  MPI_Status estado; // Variable necesaria para almacenar la información del mensaje recibido
  while(ack == 0) { // Este while va a estar ciclando mientras no le haya llegado el mensaje al siguiente en el anillo (se le acabó el timeout porque esta muerto, o bien porque estaba ocupado con otra cosa)
    flag_msg = 0; // En cada iteración reinicializamos el flag en 0 (false)
    MPI_Isend(&buffer, 2, MPI_INT,// Envío por buffer dos valores de tipo MPI_INT (la tupla <ID, ID>)
              siguiente,          // A quién se lo envío
              TAG_ELECCION,       // La clasificación del mensaje
              MPI_COMM_WORLD,     // El comm por defecto que tiene a todos los procesos del 0 al N-1, con N = total de procesos
              &request            // Identifica la comunicación entre los procesos que intervienen en la misma
              );
    int tiempo_maximo_ack = MPI_Wtime() + t; // Calculo el tiempo máximo de espera para esperar el ACK
    int ahora = MPI_Wtime(); // Guardo el tiempo actual para poder comparar y determinar si me pasé del timeout o no
    while(!flag_msg && ahora <= tiempo_maximo_ack) { // Este while va a estar ciclando mientras no se haya recibido un mensaje y no se haya pasado el tiempo máximo para el ACK
      MPI_Iprobe(siguiente,       // De quién voy a ver si me envio un mensaje
                 TAG_ACK,         // La clasificación del mensaje
                 MPI_COMM_WORLD,  // El comm por defecto que tiene a todos los procesos del 0 al N-1, con N = total de procesos
                 &flag_msg,       // El flag para determinar si hay o no un mensaje
                 &estado          // Guarda información relevante como el rank del proceso que envió el ACK
                 );
      ahora = MPI_Wtime(); // Actualizo la hora actual para usar como condición de corte (timeout) junto con el flag
    }
    siguiente = siguiente_pid(siguiente, 0); // Sirve sólo si tengo que enviarle un mensaje al siguiente proceso porque el actual no respondió el ACK
    if(ahora > tiempo_maximo_ack) continue; // Si salí del while por timeout, volver a ciclar para enviar el mensaje al siguiente proceso
    printf("Soy: %d y recibi ACK de: %d\n", pid, estado.MPI_SOURCE); // Sólo para testear cosillas
    double respuesta; // Oh, hemos declarado una variable 'respuesta' para almacenar el mensaje ACK (NO DEBERIA SER ARRAY DE CHARS QUE CONTENGAN "OK"?!)
    MPI_Irecv(&respuesta,         // Donde guardamos el mensaje que llega
              1,                  // Cuántos datos me llegan
              MPI_DOUBLE,         // Qué tipo de dato me llega
              estado.MPI_SOURCE,  // De quién lo recibo
              TAG_ACK,            // La clasificación del mensaje
              MPI_COMM_WORLD,     // El comm por defecto que tiene a todos los procesos del 0 al N-1, con N = total de procesos
              &request            // Identifica la comunicación entre los procesos que intervienen en la misma
              );
    if(respuesta <= tiempo_maximo_ack) ack = 1; // Si llego el OK del ACK entonces pongo ack en true para salir del ciclo. DEBERIA CHECKEAR POR OK EN LUGAR DEL TIEMPO
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
                 &flag_msg, //Si recibí un mensaje se pone en 1
                 &estado //Me dice quién y con que tag me mandó el mensaje
                 );
      ahora = MPI_Wtime();
    }
    if(ahora >= tiempo_maximo) break;
    /***Le aviso al proceso que me mandó la tupla que la recibí***/
    printf("Soy: %d, recibi mensaje de: %d y le respondí.\n", pid, estado.MPI_SOURCE);
    char ok[] = {'O', 'K'};
    MPI_Isend(&ok, //Donde está el mensaje que mandamos
          2, //Cuántos datos envío
          MPI_CHAR, //Qué tipo de dato envío
          estado.MPI_SOURCE, //Le respondo a quien me envío la tupla
          TAG_ACK, 
          MPI_COMM_WORLD, 
          &request
          );
    
    int buffer[2];
    MPI_Irecv(&buffer, //Donde guardamos el mensaje que llega
              2, //Cuántos datos me llegan
              MPI_INT,  //Qué tipo de dato me llega
              estado.MPI_SOURCE, 
              TAG_ELECCION, 
              MPI_COMM_WORLD, 
              &request
              );
    
    int i = buffer[0];
    int cl = buffer[1];
    if(i == pid) { //Dio toda la vuelta
      if(cl > pid) { //El líder está adelante, hay que avisarle
        buffer[0] = cl; //El par es <cl,cl>
      } else { //El líder soy yo
        status = LIDER;
        continue;
      }
    } else { //No dio toda la vuelta
      if(pid > cl) { //Nuevo líder potencial
        buffer[1] = pid;
      }
    }

    int ack = 0;
    int flag_msg_ack = 0;
    MPI_Status estado_ack;
    proximo = siguiente_pid(pid, es_ultimo);
    while(ack == 0){
      flag_msg_ack = 0;
      MPI_Isend(&buffer, //Donde está el mensaje que mandamos
                2, //Cuántos datos envío
                MPI_INT, //Qué tipo de dato envío
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
                2,                        //Cuántos datos me llegan
                MPI_CHAR,               //Qué tipo de dato me llega
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
 printf("Proceso %u %s líder.\n", pid, (status==LIDER ? "es" : "no es"));
}
