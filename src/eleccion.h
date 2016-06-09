#ifndef	_ELECCION_H
#define	_ELECCION_H

/* Enumerado para ver si soy o no l�der. */
enum status { NO_LIDER, LIDER };
typedef enum status t_status;

/* PID de elecci�n de l�der. */
typedef unsigned short int t_pid;
#define	MPI_PID	MPI_UNSIGNED_SHORT

/* Funci�n que maneja la elecci�n de l�der por no m�s
 * de timeout segundos.
 * pid es el PID del proceso local.
 * es_ultimo indica si �ste es el �ltimo proceso del anillo o no.
 */
void eleccion_lider(t_pid pid, int es_ultimo, unsigned int timeout);

/* Funci�n que se encarga de iniciar la elecci�n de l�der.
 * Los par�metros tienen la misma sem�ntica que eleccion_lider().
 */
void iniciar_eleccion(t_pid pid, int es_ultimo);

#endif	/* ELECCION_H */
 