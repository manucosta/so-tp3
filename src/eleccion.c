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
  static t_status status= NO_LIDER; // Creo una variable static con el valor NO_LIDER ya que al inicio no sos el líder
  double ahora= MPI_Wtime(); // La variable 'ahora' guarda el tiempo actual. Va a servir para comparar si me fui del timeout
  double tiempo_maximo= ahora+timeout; // Calculo el tiempo máximo que tengo antes que llegue el timeout
  t_pid proximo = siguiente_pid(pid, es_ultimo); // Obtengo el siguiente pid al cual le tengo que enviar el mensaje
  int flag_msg; // Variable de control
  MPI_Request request; // Variable necesaria para invocar al MPI_Isend. Allí se guardaran datos sobre la comunicación pero no nos importan en este caso

  while (ahora<tiempo_maximo) { // Ciclamos mientras no me haya excedido con el timeout
    flag_msg = 0; // Reinicio el flag en 0
    MPI_Status estado; // Variable necesaria para invocar al MPI_Iprobe. Provee datos sobre quién me envió el mensaje y con qué tag
    while(!flag_msg && ahora < tiempo_maximo) { // Mientras no haya recibido un mensaje y no me haya pasado del timeout...
      // Verifico que me llegó un mensaje
      MPI_Iprobe(MPI_ANY_SOURCE, // Espero un mensaje de cualquier fuente
                 TAG_ELECCION,   // El tag es ELECCION
                 MPI_COMM_WORLD, // El comm default que contiene a todos los procesos
                 &flag_msg,      // Si recibí un mensaje se pone en 1
                 &estado         // Guarda información relevante como el rank del proceso que envió el mensaje
                 );
      ahora = MPI_Wtime(); // Actualizo el tiempo actual
    }
    if(ahora >= tiempo_maximo) break; // Si corte el while anterior por timeout, salgo del while grande porque ya me excedí del tiempo permitido
    // Si sigue por acá abajo, entonces no me excedí del timeout. Tengo que avisar que me llegó el mensaje (dar el ACK)
    /***Le aviso al proceso que me mandó la tupla que la recibí***/
    printf("Soy: %d, recibi mensaje de: %d y le respondí.\n", pid, estado.MPI_SOURCE);
    char ok[] = {'O', 'K'}; // Creo un mensaje para enviar mi ACK
    // Envío mi ACK
    MPI_Isend(&ok,           // El mensaje que voy a enviar
          2,                 // Cuántos datos envío
          MPI_CHAR,          // Qué tipo de dato envío
          estado.MPI_SOURCE, // Le respondo a quien me envío la tupla
          TAG_ACK,           // Uso el tag ACK para enviar el mensaje
          MPI_COMM_WORLD,    // Uso el comm default que numera a los procesos con rank de 0 a N-1, siendo N el total de procesos
          &request           // Uso el request que cree al inicio de la función
          );
    
    int buffer[2]; // Creo una variable para almacenar el mensaje que me llegó
    MPI_Irecv(&buffer,           // Donde guardamos el mensaje que llega
              2,                 // Cuántos datos me llegan
              MPI_INT,           // Qué tipo de dato me llega
              estado.MPI_SOURCE, // Recibo el mensaje del proceso correcto
              TAG_ELECCION,      // Recibo por el tag ELECCION
              MPI_COMM_WORLD,    // Uso el comm default que numera a los procesos con rank de 0 a N-1, siendo N el total de procesos
              &request           // Uso el request que cree al inicio de la función
              );
    
    int i = buffer[0]; // Guardo el primer item de la tupla
    int cl = buffer[1]; // Guardo el degundo item de la tupla
    // Hago la lógica que menciona el enunciado del TP
    if(i == pid) { // Dio toda la vuelta
      if(cl > pid) { // El líder está adelante, hay que avisarle
        buffer[0] = cl; // El par es <cl,cl>
      } else { // El líder soy yo
        status = LIDER; // Me pongo como lider
        continue; // Continúo ciclando
      }
    } else { // No dió toda la vuelta
      if(pid > cl) { // Nuevo líder potencial
        buffer[1] = pid; // El par es <i,pid>
      }
    }

    int ack = 0; // Creo una variable ack. Sirve para saber si me llego el ACK indicado
    int flag_msg_ack = 0; // Creo una variable flag_msg_ack. Sirve para saber si ya se recibió el mensaje que envié
    MPI_Status estado_ack; // Esta variable es necesaria para obtener información sobre la comunicación
    proximo = siguiente_pid(pid, es_ultimo); // Calculo el proximo pid al cual le tengo que enviar la tupla
    while(ack == 0) { // Mientras no haya recibido el ACK indicando que se recibió la tupla que envié...
      flag_msg_ack = 0; // Reinicio el flag en 0
      MPI_Isend(&buffer,        // Donde está el mensaje que mandamos
                2,              // Cuántos datos envío
                MPI_INT,        // Qué tipo de dato envío
                proximo,        // A quién se lo envío
                TAG_ELECCION,   // Qué tag uso. En este caso el tag ELECCION porque estoy enviando la tupla para poder elegir al líder
                MPI_COMM_WORLD, // Qué comm uso. En este caso uso el comm por defecto
                &request        // La variable que uso para almacenar la información referente a la comunicación entre los procesos involucrados
                );
      printf("Soy: %d y envie a: %d\n", pid, proximo);
      int tiempo_maximo_ack = MPI_Wtime() + t; // Calculo el tiempo máximo que puedo esperar a que llegue el ACK antes de enviarle el mensaje al próximo en el anillo
      ahora = MPI_Wtime(); // Obtengo el tiempo actual
      while(!flag_msg_ack && ahora <= tiempo_maximo_ack) { // Ciclo mientras no haya recibido el ACK y no me haya excedido del timeout
        MPI_Iprobe(proximo,        // Espero un mensaje de 'proximo'
                   TAG_ACK,        // Con el tag ACK
                   MPI_COMM_WORLD, // Usando el comm por defecto
                   &flag_msg_ack,  // Si recibí un mensaje se pone en 1
                   &estado_ack     // Guarda información relevante como el rank del proceso que envió el ACK
                   );
        ahora = MPI_Wtime(); // Actualizo el tiempo actual para seguir ciclando
      }
      int asd = proximo;//sacar despues
      proximo = siguiente_pid(proximo, 0); // Obtengo el proximo pid al cual enviar el mensaje
      if(ahora > tiempo_maximo_ack) { // Si me excedí del timeout, seguir ciclando para enviarle el mensaje al siguiente pid
        printf("Soy: %d y %d no me respondio. Envio a: %d\n", pid, asd, proximo);
        continue;
      }
      // Si llega hasta acá, entonces no me excedí del timeout. Entonces verifico que el ACK que me llegó sea correcto y termino de ciclar
      //printf("Soy: %d y recibi mensaje de: %d\n", pid, estado_ack.MPI_SOURCE);
      char respuesta[] = {'K', 'O'}; // Variable para almacenar el ACK
      MPI_Irecv(&respuesta,            // Donde guardamos el mensaje que llega
                2,                     // Cuántos datos me llegan
                MPI_CHAR,              // Qué tipo de dato me llega
                estado_ack.MPI_SOURCE, // De quién recibo el mensaje
                TAG_ACK,               // Qué tag estoy usando. En este caso el ACK
                MPI_COMM_WORLD,        // Uso el comm por defecto
                &request               // Guardo la información sobre la comunicación en la variable 'request'
                );
      if(respuesta[0] == 'O' && respuesta[1] == 'K') ack = 1; // Si el ACK es el correcto, pongo 'ack' en 1 para que termine de ciclar
    }
    ahora= MPI_Wtime(); // Actualizo el tiempo actual
  }

 /* Reporto mi status al final de la ronda. */
 printf("Proceso %u %s líder.\n", pid, (status==LIDER ? "es" : "no es"));
}
