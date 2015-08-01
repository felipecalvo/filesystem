#include "Funciones_Filesystem.h"

char* PUERTO_LISTEN;
char* IP_LISTEN;
int CANTIDAD_NODOS;

int socket_cliente_marta;
int socket_servidor;
int socket_global_a_buscar = 0;
char *nombre_global_a_buscar;
int bloque_global_a_buscar;
int padre_global_a_buscar;

pthread_mutex_t mutex_socket_global = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_nombre_global = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bloque_global = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_padre_global = PTHREAD_MUTEX_INITIALIZER;

pthread_t enviar_copia_uno;
pthread_t enviar_copia_dos;
pthread_t enviar_copia_tres;

t_config* config;

un_directorio *sistema[1024];
int ocupado[1024] = {1};
un_directorio *raiz;

t_list *archivos;

t_list *nodos;

t_list *nodos_para_marta;

t_list *bloques;

t_list *bloques_copiandose;

t_list *bloques_pedidos_por_marta;

t_log* logger;

t_log* logger_bloques;



int main(void) {

	inicializar();

//	printf("el archivo de /ivo/caca.txt es %s\n", obtener_archivo_de_ruta("/ivo/caca.txt", strlen("/ivo/caca.txt" + 1)));
//	printf("el archivo de /ivo.txt es %s\n", obtener_archivo_de_ruta("/ivo.txt", strlen("/ivo.txt" + 1)));
//	printf("el archivo de .txt es %s\n", obtener_archivo_de_ruta("ivo.txt", strlen("ivo.txt" + 1)));
//	printf("el archivo de /ivo/caca/fiesta.txt es %s\n", obtener_archivo_de_ruta("/ivo/caca/fiesta.txt", strlen("/ivo/caca/fiesta.txt" + 1)));
//	printf("el archivo de ../../test.txt es %s\n", obtener_archivo_de_ruta("../../../test.txt", strlen("../../../test.txt" + 1)));

	socket_servidor = crearSocketServidor(PUERTO_LISTEN);

	conectar_nodos(socket_servidor);
	socket_cliente_marta = conectar_marta(socket_servidor);

	char* iniciado = malloc(100);
	sprintf(iniciado, "Filesystem iniciado.\n");
	log_info(logger, iniciado);
	free(iniciado);


	//pthread_t hilo_manejar_consola;
	//pthread_create(&hilo_manejar_consola, NULL, (void*) consola, NULL);

	consola();

	list_destroy_and_destroy_elements(archivos, (void *) destructor_archivo);
	list_destroy_and_destroy_elements(nodos, (void *) destructor_nodo);
//	list_destroy_and_destroy_elements(bloques, (void *) destructor_bloque);
//	list_destroy_and_destroy_elements(nodos_para_marta, (void *) nodos_para_marta);

	config_destroy(config);

	free(raiz);

	return 0;

}


