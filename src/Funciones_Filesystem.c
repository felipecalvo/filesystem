#include "Funciones_Filesystem.h"

extern char* PUERTO_LISTEN;
extern char* IP_LISTEN;
extern int CANTIDAD_NODOS;

extern int socket_cliente_marta;
extern int socket_servidor;
extern int socket_global_a_buscar;
extern char* nombre_global_a_buscar;
extern int bloque_global_a_buscar;
extern int padre_global_a_buscar;

extern pthread_mutex_t mutex_socket_global;
extern pthread_mutex_t mutex_nombre_global;
extern pthread_mutex_t mutex_bloque_global;
extern pthread_mutex_t mutex_padre_global;

extern pthread_t enviar_copia_uno;
extern pthread_t enviar_copia_dos;
extern pthread_t enviar_copia_tres;


extern t_config* config;
extern un_directorio *sistema[1024];	//guardará las direcciones de, máximo, 1024 directorios
extern un_directorio *raiz;
extern int ocupado[1024];

extern t_list *archivos;
extern t_list *nodos;
extern t_list *bloques;
extern t_list *bloques_copiandose;
extern t_list *bloques_pedidos_por_marta;
extern t_list *nodos_para_marta;
extern t_log* logger;
extern t_log* logger_bloques;


void inicializar()
{
	printf("Cargando configuración...\n");
	config = config_create("config.cfg");

	PUERTO_LISTEN = config_get_string_value(config, "PUERTO_LISTEN");	//puerto pa escuchar
	IP_LISTEN = config_get_string_value(config, "IP_LISTEN"); //ip pa escuchar
	CANTIDAD_NODOS = config_get_int_value(config, "CANTIDAD_NODOS"); // minima para estar operativo

	logger = log_create("filesystem.log", "Filesystem", true, LOG_LEVEL_INFO);
	logger_bloques = log_create("bloques.log", "Filesystem", false, LOG_LEVEL_INFO);

	char* mensaje = malloc(100);
	sprintf(mensaje, "Iniciando filesystem...\n");
	log_info(logger, mensaje);
	free(mensaje);

	archivos = list_create();
	nodos = list_create();
	nodos_para_marta = list_create();
	bloques = list_create();
	bloques_copiandose = list_create();
	bloques_pedidos_por_marta = list_create();

	raiz = malloc(sizeof(un_directorio));
	raiz->directorio = "/";
	raiz->padre = 0;
	raiz->posicion = 0;

	sistema[0] = raiz;
	ocupado[0] = 1;

	FILE *archivo_directorios;
	archivo_directorios = fopen("prstdir.bin", "rb");

	if (archivo_directorios == NULL)	//si no hay ningún archivo, lo creo y le grabo la raíz
	{
		printf("No se ha encontrado un filesystem existente.\n");

		FILE* datosW;
		datosW = fopen("prstdir.bin", "wb");

		fwrite(&ocupado[0], sizeof(int), 1, datosW); //cuántos ocupados hay: uno en este momento

		size_t largo;

		largo = strlen(sistema[0]->directorio) + 1;
		fwrite(&largo, sizeof(largo), 1, datosW);
		fwrite(sistema[0]->directorio, largo, 1, datosW);

		fwrite(&sistema[0]->padre, sizeof(sistema[0]->padre), 1, datosW);
		fwrite(&sistema[0]->posicion, sizeof(sistema[0]->posicion), 1, datosW);

		fclose(datosW);	//cargar archivos
	}
	else	//y si sí hay un archivo... lo leo, amigo
	{
		fclose(archivo_directorios);
		printf("Cargando filesystem existente...\n");
		cargar_directorios();
		printf("Filesystem cargado.\n");
	}


	FILE *archivo_nodos;
	archivo_nodos = fopen("prstnod.bin", "rb");

	if (archivo_nodos == NULL)
		printf("No se ha encontrado información sobre nodos anteriores.\n");
	else
	{
		fclose(archivo_nodos);
		printf("Cargando información anterior sobre nodos...\n");
		cargar_nodos();
		printf("Información cargada.\n");
	}

	if (archivo_directorios != NULL && archivo_nodos != NULL)
	{
		FILE *archivo_archivos;
		archivo_archivos = fopen("prstarc.bin", "rb");
		
		if (archivo_archivos != NULL)
		{
			fclose(archivo_archivos);
			cargar_archivos();
		}
	}

}

void cargar_directorios() //o sea leer
{
	size_t largo;

	FILE* leer_directorios;
	leer_directorios = fopen("prstdir.bin", "rb");

	int cuantos_directorios = 0;
	fread(&cuantos_directorios, sizeof(int), 1, leer_directorios);	//primero, cuántos directorios tengo que leer;

	int hasta = 0;

	while(hasta != cuantos_directorios)	//y ahora me leo todos esos
	{
		un_directorio *esto = malloc(sizeof(*esto));

		fread(&largo, sizeof(largo), 1, leer_directorios);
		esto->directorio = malloc(largo);
		fread(esto->directorio, 1, largo, leer_directorios);

		fread(&esto->padre, sizeof(esto->padre), 1, leer_directorios);
		fread(&esto->posicion, sizeof(esto->posicion), 1, leer_directorios);

		sistema[esto->posicion] = esto;
		ocupado[esto->posicion] = 1;

		hasta++;
	}

	fclose(leer_directorios);
}

void cargar_nodos()
{
	int cuantos_nodos = 0;

	FILE* leer_nodos;
	leer_nodos = fopen("prstnod.bin", "rb");

	fread(&cuantos_nodos, sizeof(int), 1, leer_nodos);

	int i = 0;
	for(i = 1; i <= cuantos_nodos; i++)
	{
		nodo *nodo_actual = malloc(sizeof(nodo));

		int largo_nombre = 0;
		fread(&largo_nombre, sizeof(largo_nombre), 1, leer_nodos);
		nodo_actual->nombre = malloc(largo_nombre);
		fread(nodo_actual->nombre, largo_nombre, 1, leer_nodos);

		int largo_ip = 0;
		fread(&largo_ip, sizeof(largo_ip), 1, leer_nodos);
		nodo_actual->ip = malloc(largo_ip);
		fread(nodo_actual->ip, largo_ip, 1, leer_nodos);

		int largo_puerto = 0;
		fread(&largo_puerto, sizeof(largo_puerto), 1, leer_nodos);
		nodo_actual->puerto = malloc(largo_puerto);
		fread(nodo_actual->puerto, largo_puerto, 1, leer_nodos);

		nodo_actual->estado = false;

		int espacio_total = 0;
		fread(&espacio_total, 1, sizeof(espacio_total), leer_nodos);
		nodo_actual->espacio_total = espacio_total;

		int espacio_utilizado = 0;
		fread(&espacio_utilizado, 1, sizeof(espacio_utilizado), leer_nodos);
		nodo_actual->espacio_utilizado = espacio_utilizado;

		nodo_actual->socket = 0;

		list_add(nodos, nodo_actual);

		list_add(nodos_para_marta, nodo_actual);

	}

}

void cargar_archivos()
{
	FILE* leer_archivos;
	leer_archivos = fopen("prstarc.bin", "rb");

	int cuantos_archivos = 0;
	fread(&cuantos_archivos, sizeof(int), 1, leer_archivos);

	int i = 0;
	for(i = 0; i < cuantos_archivos; i++)
	{
		archivo *archivo_actual = malloc(sizeof(archivo));

		int largo_nombre_archivo = 0;
		fread(&largo_nombre_archivo, sizeof(largo_nombre_archivo), 1, leer_archivos);
		archivo_actual->nombre = malloc(largo_nombre_archivo);
		fread(archivo_actual->nombre, largo_nombre_archivo, 1, leer_archivos);

		int padre = 0;
		fread(&padre, sizeof(padre), 1, leer_archivos);
		archivo_actual->padre = padre;

		float tamanio = 0;
		fread(&tamanio, sizeof(tamanio), 1, leer_archivos);
		archivo_actual->tamanio = tamanio;
		
		int cuantos_bloques = 0;
		fread(&cuantos_bloques, sizeof(int), 1, leer_archivos);

		archivo_actual->estado = true;

		archivo_actual->lista_bloques_datos = list_create();
		
		int j = 0;
		for(j = 0; j < cuantos_bloques; j++)
		{
			bloque_datos *bloque_actual = malloc(sizeof(bloque_datos));
			
			int num_bloque_archivo = 0;
			fread(&num_bloque_archivo, sizeof(num_bloque_archivo), 1, leer_archivos);
			bloque_actual->num_bloque_archivo = num_bloque_archivo;
			
			int largo_nombre_nodo = 0;
			fread(&largo_nombre_nodo, sizeof(largo_nombre_nodo), 1, leer_archivos);
			bloque_actual->nodo_padre = malloc(largo_nombre_nodo);
			fread(bloque_actual->nodo_padre, largo_nombre_nodo, 1, leer_archivos);
			
			int bloque_nodo_padre = 7;
			fread(&bloque_nodo_padre, sizeof(bloque_actual->bloque_nodo_padre), 1, leer_archivos);
			bloque_actual->bloque_nodo_padre = bloque_nodo_padre;

			list_add(archivo_actual->lista_bloques_datos, bloque_actual);
			list_add(bloques, bloque_actual);

		}

		list_add(archivos, archivo_actual);

	}

	fclose(leer_archivos);
}


void persistir_directorios() //o sea escribir
{
	FILE* datosW;
	datosW = fopen("prstdir.bin", "wb");

	int cuantos = cuantos_directorios_ocupados();
	fwrite(&cuantos, sizeof(int), 1, datosW); 	//primero, cuántos voy a grabar en total.
												//no le compliquemos la vida al que tiene que leer.
	int i = 0;
	while (i <= 1023)
	{
		if (ocupado[i] == 1)
		{
			size_t largo;
			largo = strlen(sistema[i]->directorio) + 1;
			fwrite(&largo, sizeof(largo), 1, datosW);
			fwrite(sistema[i]->directorio, largo, 1, datosW);

			fwrite(&sistema[i]->padre, sizeof(sistema[i]->padre), 1, datosW);
			fwrite(&sistema[i]->posicion, sizeof(sistema[i]->posicion), 1, datosW);

		}

		i++;
	}

	fclose(datosW);
}

void persistir_nodos() //o sea escribir
{
	FILE* datosW;
	datosW = fopen("prstnod.bin", "wb");

	int cuantos = list_size(nodos);
	fwrite(&cuantos, sizeof(int), 1, datosW); 	//primero, cuántos voy a grabar en total.
												//no le compliquemos la vida al que tiene que leer.
	int i = 0;
	for(i = 0; i < cuantos; i++)
	{
		nodo *nodo_actual = list_get(nodos, i);

		size_t largo_nombre = strlen(nodo_actual->nombre) + 1;
		size_t largo_ip = strlen(nodo_actual->ip) + 1;
		size_t largo_puerto = strlen(nodo_actual->puerto) + 1;

		fwrite(&largo_nombre, sizeof(largo_nombre), 1, datosW);
		fwrite(nodo_actual->nombre, largo_nombre, 1, datosW);

		fwrite(&largo_ip, sizeof(largo_ip), 1, datosW);
		fwrite(nodo_actual->ip, largo_ip, 1, datosW);

		fwrite(&largo_puerto, sizeof(largo_puerto), 1, datosW);
		fwrite(nodo_actual->puerto, largo_puerto, 1, datosW);

		fwrite(&nodo_actual->espacio_total, sizeof(nodo_actual->espacio_total), 1, datosW);
		fwrite(&nodo_actual->espacio_utilizado, sizeof(nodo_actual->espacio_utilizado), 1, datosW);

	}

	fclose(datosW);
}

void persistir_archivos() //o sea escribir
{
	FILE* datosW;
	datosW = fopen("prstarc.bin", "wb");

	int cuantos_archivos = list_size(archivos);
	fwrite(&cuantos_archivos, sizeof(int), 1, datosW); 	//primero, cuántos voy a grabar en total.
												//no le compliquemos la vida al que tiene que leer.
	int i = 0;
	for(i = 0; i < cuantos_archivos; i++)
	{
		archivo *archivo_actual = list_get(archivos, i);

		size_t largo_nombre = strlen(archivo_actual->nombre) + 1;
		fwrite(&largo_nombre, sizeof(largo_nombre), 1, datosW);
		fwrite(archivo_actual->nombre, largo_nombre, 1, datosW);

		fwrite(&archivo_actual->padre, sizeof(archivo_actual->padre), 1, datosW);
		fwrite(&archivo_actual->tamanio, sizeof(archivo_actual->tamanio), 1, datosW);
		
		int cuantos_bloques = list_size(archivo_actual->lista_bloques_datos);
		fwrite(&cuantos_bloques, sizeof(int), 1, datosW);
		
		int j = 0;
		for(j = 0; j < cuantos_bloques; j++)
		{
			bloque_datos *bloque_actual = list_get(archivo_actual->lista_bloques_datos, j);
			
			fwrite(&bloque_actual->num_bloque_archivo, sizeof(bloque_actual->num_bloque_archivo), 1, datosW);
			
			size_t largo_nombre_nodo = strlen(bloque_actual->nodo_padre) + 1;
			fwrite(&largo_nombre_nodo, sizeof(largo_nombre_nodo), 1, datosW);
			fwrite(bloque_actual->nodo_padre, largo_nombre_nodo, 1, datosW);
			
			fwrite(&bloque_actual->bloque_nodo_padre, sizeof(bloque_actual->bloque_nodo_padre), 1, datosW);
		}

	}

	fclose(datosW);
}

int conectar_marta(int socket_servidor)
{
	printf("%s", "Esperando conexión de MaRTA... \n");

	listen(socket_servidor, 1);
	socket_cliente_marta = aceptarConexion(socket_servidor);

	if(socket_cliente_marta)
	{
		printf("MaRTA conectado. \n");
		pthread_t hilo_mantener_marta;
		pthread_create(&hilo_mantener_marta, NULL, (void*) mantener_conexion_marta, (void*) socket_cliente_marta);
	}

	return socket_cliente_marta;
}

void mantener_conexion_marta(int socket_cliente_marta)
{
	int operacion;

	while( recv(socket_cliente_marta, &operacion, sizeof(uint32_t), 0) > 0 )
	{
		switch(operacion)
		{
			case 16: //obtener bloques
				marta_obtener_bloques(socket_cliente_marta);
				break;

			case 17: //solicitar_nodos
				marta_solicitar_nodos();
				break;

			case 20: //solicitar_resultado
				marta_solicitar_resultado();
				break;

			case 22: //solicitar_nodos_caidos
				marta_solicitar_nodos_caidos();
				break;

		}
	}
}

void marta_obtener_bloques(int socket_cliente_marta)
{
	uint32_t largo_ruta = 0;
	recv(socket_cliente_marta, &largo_ruta, sizeof(uint32_t), MSG_WAITALL);

	char *ruta_archivo = malloc(largo_ruta);
	recv(socket_cliente_marta, ruta_archivo, largo_ruta, MSG_WAITALL);

//	printf("recibo una ruta %s de largo %d", ruta_archivo, largo_ruta);

	char *nombre_archivo = obtener_archivo_de_ruta(ruta_archivo, largo_ruta);

	archivo *archivo = devolver_este_archivo(nombre_archivo);

	if( devolver_este_archivo(nombre_archivo) == 0 )
	{
		printf("No existe esa ruta.");
		free(nombre_archivo);
		return;
	}

	uint32_t cantidad_de_bloques = cuantos_bloques_tiene(archivo);

	uint32_t size_total_para_enviar = sizeof(cantidad_de_bloques);
	char* paquete_serializado = malloc(size_total_para_enviar);

	int offset = 0;

	uint32_t size_to_send = sizeof(cantidad_de_bloques);
	memcpy(paquete_serializado+offset, &cantidad_de_bloques, size_to_send);
	offset += size_to_send;
	//size_total_para_enviar += size_to_send;

	int i = 0;
	for(i = 1; i <= cantidad_de_bloques; i++)
	{

		bloque_global_a_buscar = i;
		t_list *lista_para_este_bloque = list_filter(archivo->lista_bloques_datos, (void *) es_copia_de_este_bloque);

		int j = 0;
		uint32_t cantidad_de_copias = 0;
		for(j = 0; j < list_size(lista_para_este_bloque); j++)
		{
			bloque_datos *bloque_actual = list_get(lista_para_este_bloque, j);

			nodo *nodo_conectado = obtener_nodo_por_nombre(bloque_actual->nodo_padre);

			if(nodo_conectado->estado == true)
			{
				cantidad_de_copias++;
			}
		}

		size_to_send = sizeof(cantidad_de_copias);
		paquete_serializado = realloc(paquete_serializado, offset + size_to_send);
		memcpy(paquete_serializado+offset, &cantidad_de_copias, size_to_send);
		offset += size_to_send;
		size_total_para_enviar += size_to_send;

		for(j = 0; j < list_size(lista_para_este_bloque); j++)
		{
			bloque_datos *bloque_actual = list_get(lista_para_este_bloque, j);

			nodo *nodo_conectado = obtener_nodo_por_nombre(bloque_actual->nodo_padre);

			if(nodo_conectado->estado == true)
			{
				uint32_t bloque_en_nodo = bloque_actual->bloque_nodo_padre;
				uint32_t largo_nombre_nodo = strlen(bloque_actual->nodo_padre) + 1;

				size_to_send = sizeof(bloque_en_nodo);
				paquete_serializado = realloc(paquete_serializado, offset + size_to_send);
				memcpy(paquete_serializado+offset, &bloque_en_nodo, size_to_send);
				offset += size_to_send;
				size_total_para_enviar += size_to_send;

				size_to_send = sizeof(largo_nombre_nodo);
				paquete_serializado = realloc(paquete_serializado, offset + size_to_send);
				memcpy(paquete_serializado+offset, &largo_nombre_nodo, size_to_send);
				offset += size_to_send;
				size_total_para_enviar += size_to_send;

				size_to_send = largo_nombre_nodo;
				paquete_serializado = realloc(paquete_serializado, offset + size_to_send);
				memcpy(paquete_serializado+offset, bloque_actual->nodo_padre, size_to_send);
				offset += size_to_send;
				size_total_para_enviar += size_to_send;
			}
		}
	}

	enviar_datos(socket_cliente_marta, paquete_serializado, size_total_para_enviar);

	free(paquete_serializado);
	free(ruta_archivo);
	free(nombre_archivo);
}


uint32_t size_lista_nodos()
{
	int cantidad_nodos = list_size(nodos_para_marta);

	uint32_t size_total = 0;

	int i = 0;

	for(; i < cantidad_nodos; i++)
	{
		nodo* nodo_actual = (nodo*) list_get(nodos, i);

		uint32_t size_nombre = strlen(nodo_actual->nombre) + 1;
		uint32_t size_ip = strlen(nodo_actual->ip) + 1;
		uint32_t size_puerto = strlen(nodo_actual->puerto) + 1;

		size_total += (sizeof(size_nombre) + size_nombre + sizeof(size_ip) + size_ip + sizeof(size_puerto) + size_puerto);
	}

	return size_total;
}


void marta_solicitar_nodos()
{
	uint32_t offset = 0;
	uint32_t size_total_para_enviar = sizeof(uint32_t) + size_lista_nodos();
	uint32_t size_to_send;
	uint32_t cantidad_de_nodos = list_size(nodos_para_marta);

	char* paquete_serializado = malloc(size_total_para_enviar);

	size_to_send = sizeof(cantidad_de_nodos);
	memcpy(paquete_serializado+offset, &cantidad_de_nodos, size_to_send);
	offset += size_to_send;

	int i = 0;
	for(; i < cantidad_de_nodos; i++)
	{

		nodo* nodo_actual = list_get(nodos_para_marta, i);

		uint32_t largo_nombre_nodo = strlen(nodo_actual->nombre) + 1;

		size_to_send = sizeof(largo_nombre_nodo);
		memcpy(paquete_serializado+offset, &largo_nombre_nodo, size_to_send);
		offset += size_to_send;

		size_to_send = largo_nombre_nodo;
		memcpy(paquete_serializado+offset, nodo_actual->nombre, size_to_send);
		offset += size_to_send;


		uint32_t largo_ip_nodo = strlen(nodo_actual->ip) + 1;

		size_to_send = sizeof(largo_ip_nodo);
		memcpy(paquete_serializado+offset, &largo_ip_nodo, size_to_send);
		offset += size_to_send;

		size_to_send = largo_ip_nodo;
		memcpy(paquete_serializado+offset, nodo_actual->ip, size_to_send);
		offset += size_to_send;


		uint32_t largo_puerto_nodo = strlen(nodo_actual->puerto) + 1;

		size_to_send = sizeof(largo_puerto_nodo);
		memcpy(paquete_serializado+offset, &largo_puerto_nodo, size_to_send);
		offset += size_to_send;

		size_to_send = largo_puerto_nodo;
		memcpy(paquete_serializado+offset, nodo_actual->puerto, size_to_send);
		offset += size_to_send;

	}

	enviar_datos(socket_cliente_marta, paquete_serializado, size_total_para_enviar);

	list_clean(nodos_para_marta);
	free(paquete_serializado);
}

//Recibe una lista de nombres de nodos de MaRTA, la filtra y devuelve una lista de nombres de nodos caidos
void marta_solicitar_nodos_caidos()
{
	int i;
	uint32_t largo_nombre_nodo;
	uint32_t cantidad_nodos_a_chequear = 0;
	t_list* nodos_recibidos = list_create();

	recv(socket_cliente_marta, &cantidad_nodos_a_chequear, sizeof(uint32_t), MSG_WAITALL);

	for( i=0; i<cantidad_nodos_a_chequear; i++)
	{
		recv(socket_cliente_marta, &largo_nombre_nodo, sizeof(uint32_t), MSG_WAITALL);

		char* nombre_nodo = malloc(largo_nombre_nodo);
		recv(socket_cliente_marta, nombre_nodo, largo_nombre_nodo, MSG_WAITALL);

		list_add(nodos_recibidos, nombre_nodo);
	}

	t_list* nodos_caidos = list_filter(nodos_recibidos, (void*) es_nodo_caido);

	uint32_t tamanio_lista_nodos_caidos;
	uint32_t cantidad_nodos_caidos = list_size(nodos_caidos);
	uint32_t tamanio_paquete;

	char* paquete;
	int offset = 0;

	if(cantidad_nodos_caidos == 0)
	{
		tamanio_paquete = sizeof(cantidad_nodos_caidos);
		paquete = malloc(tamanio_paquete);
		memcpy(paquete + offset, &cantidad_nodos_caidos, sizeof(cantidad_nodos_caidos));

		enviar_datos(socket_cliente_marta, paquete, tamanio_paquete);
	}
	else
	{
		tamanio_lista_nodos_caidos = tamanio_lista_nodos(nodos_caidos);
		tamanio_paquete = sizeof(cantidad_nodos_caidos) + tamanio_lista_nodos_caidos +
				(largo_nombre_nodo * list_size(nodos_caidos));

		paquete = malloc(tamanio_paquete);
		memcpy(paquete + offset, &cantidad_nodos_caidos, sizeof(cantidad_nodos_caidos));
		offset += sizeof(cantidad_nodos_caidos);

		for( i=0; i<list_size(nodos_caidos); i++)
		{
			char* nombre_nodo = (char*) list_get(nodos_caidos,i);
			largo_nombre_nodo = (strlen(nombre_nodo) + 1);

			memcpy(paquete + offset, &largo_nombre_nodo, sizeof(largo_nombre_nodo));
			offset += sizeof(largo_nombre_nodo);

			memcpy(paquete + offset, nombre_nodo, largo_nombre_nodo);
			offset += largo_nombre_nodo;
		}

		enviar_datos(socket_cliente_marta, paquete, tamanio_paquete);
	}

	list_destroy_and_destroy_elements(nodos_recibidos, (void*) destructor_nombre);
	list_destroy(nodos_caidos);

	return;

}

void destructor_nombre(char* nombre)
{
	free(nombre);
}

bool es_nodo_caido(char* nombre_nodo)
{
	nodo* nodo = obtener_nodo_por_nombre(nombre_nodo);

	return (!(nodo->estado));
}

uint32_t tamanio_lista_nodos(t_list* lista_nodos)
{

	uint32_t tamanio = 0;
	int i;

	for( i=0; i<list_size(lista_nodos); i++)
	{
		char* nombre_nodo = (char*) list_get(lista_nodos,i);
		tamanio += (strlen(nombre_nodo) + 1);
	}

	return tamanio;
}


void marta_solicitar_resultado()
{
//	printf("Hola MaRTA! Solicitaste resultados.\n");

	uint32_t largo_ruta = 0;
	recv(socket_cliente_marta, &largo_ruta, sizeof(uint32_t), MSG_WAITALL);

//	printf("Me mandaste una ruta de %d,\n", largo_ruta);

	char *ruta_archivo = malloc(largo_ruta);
	recv(socket_cliente_marta, ruta_archivo, largo_ruta, MSG_WAITALL);

//	printf("Que se ve así: %s.\n", ruta_archivo);

	uint32_t largo_nombre_nodo = 0;
	recv(socket_cliente_marta, &largo_nombre_nodo, sizeof(uint32_t), MSG_WAITALL);

//	printf("Y también un largo de nombre de nodo que es %d\n", largo_nombre_nodo);

	char *nombre_nodo = malloc(largo_nombre_nodo);
	recv(socket_cliente_marta, nombre_nodo, largo_nombre_nodo, MSG_WAITALL);

//	printf("Y se llama así: %s.\n", nombre_nodo);

	if(largo_ruta == 0 || largo_nombre_nodo == 0)
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "Error recibiendo datos de MaRTA.\n");
		log_info(logger, mensaje);
		free(mensaje);

		free(ruta_archivo);
		free(nombre_nodo);

		return;
	}

	uint32_t operacion = GETFILE;

	char *nombre_archivo = obtener_archivo_de_ruta(ruta_archivo, largo_ruta);
	uint32_t largo_nombre_archivo = strlen(nombre_archivo) + 1;

//	printf("Resulta que el archivo en sí se llama %s y mide %d\n", nombre_archivo, largo_nombre_archivo);

	nodo* nodo_a_pedir = obtener_nodo_por_nombre(nombre_nodo);
	int socket_nodo = nodo_a_pedir->socket;

//	printf("se lo voy a pedir al nodo %s, como me dijo marta\n", nodo_a_pedir->nombre);

	uint32_t size_total_para_enviar = sizeof(operacion) + sizeof(largo_nombre_archivo) + largo_nombre_archivo;
	char* paquete_serializado = malloc(size_total_para_enviar);

	int offset = 0;

	uint32_t size_to_send = sizeof(operacion);
	memcpy(paquete_serializado+offset, &operacion, size_to_send);
	offset += size_to_send;

	size_to_send = sizeof(largo_nombre_archivo);
	memcpy(paquete_serializado+offset, &largo_nombre_archivo, size_to_send);
	offset += size_to_send;

	size_to_send = largo_nombre_archivo;
	memcpy(paquete_serializado+offset, nombre_archivo, size_to_send);
	offset += size_to_send;

//	printf("le envío al nodo un paquete: %d + %d + %s\n", operacion, largo_nombre_archivo, nombre_archivo);

	int exito = enviar_datos(socket_nodo, paquete_serializado, size_total_para_enviar);

//	printf("se envió! eee\n");

	free(paquete_serializado);

	if (exito == -1)
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "Error enviando datos al nodo %s.\n", nodo_a_pedir->nombre);
		log_info(logger, mensaje);
		free(mensaje);

		free(ruta_archivo);
		free(nombre_nodo);
		free(nombre_archivo);

		return;
	}


//	printf("ahora vamos a recibir del nodis %s\n", nodo_a_pedir->nombre);

	pthread_cancel(nodo_a_pedir->hilo);

	uint32_t tamanio_de_archivo;
	int tamanio_archivo_ok = recv(socket_nodo, &tamanio_de_archivo, sizeof(tamanio_de_archivo), MSG_WAITALL);

//	printf("aparentemente el archivo pesa %d\n", tamanio_de_archivo);

	char *datos_archivo = malloc(tamanio_de_archivo);
	int datos_ok = recv(socket_nodo, datos_archivo, tamanio_de_archivo, MSG_WAITALL);

//	printf("y los datos son... muchos. no los printeo, pero llegaron\n");

	pthread_t hilo_mantener_nodo;
	pthread_create(&hilo_mantener_nodo, NULL, (void*) mantener_conexion_nodo, (void*) nodo_a_pedir->socket);
	nodo_a_pedir->hilo = hilo_mantener_nodo;

	if (tamanio_archivo_ok == -1 || datos_ok == -1)
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "Error recibiendo datos del nodo %s.\n", nodo_a_pedir->nombre);
		log_info(logger, mensaje);
		free(mensaje);
	}
	else
	{
		datos_archivo = realloc(datos_archivo, BLOCKSIZE);

		int ultimo_enter = buscar_ultimo_enter(datos_archivo, tamanio_de_archivo);

		int j = 0;
		for(j=(ultimo_enter + 1); j<BLOCKSIZE; j++)
		{
			datos_archivo[j] = '\0';
		}

//		printf("envío esto por triplicado\n");

		int exito = triplicar_copia_nodos(datos_archivo, 1, 1);

		if (exito == 0)
		{
			char* mensaje = malloc(100);
			sprintf(mensaje, "Error copiando el archivo %s.\n", nombre_archivo);
			log_info(logger, mensaje);
			free(mensaje);

			free(ruta_archivo);
			free(nombre_nodo);
			free(nombre_archivo);

			return;
		}

//		printf("bien envia2\n");

		char *nombre_padre = obtener_padre_de_archivo_de_ruta(ruta_archivo, strlen(ruta_archivo));

//		printf("el nombre de padre es %s\n", nombre_padre);

		int i = 1;
		int padre = 0;
		while (i <= 1023)
		{
			if (ocupado[i] == 1 && string_starts_with(sistema[i]->directorio, nombre_padre))
			{
				padre = i;
				break;
			}

			i++;
		}

		t_list *copia_bloques = list_create();
		list_add_all(copia_bloques, bloques_pedidos_por_marta);

		archivo* nuevo_archivo = malloc(sizeof(archivo));
		nuevo_archivo->estado = true;
		nuevo_archivo->nombre = string_duplicate(nombre_archivo);
		nuevo_archivo->padre = padre;
		nuevo_archivo->tamanio = tamanio_de_archivo;
		nuevo_archivo->lista_bloques_datos = copia_bloques;

//		printf("lesto, tenemos el archivo %s cargado\n", nuevo_archivo->nombre);
//		printf("su padre es %d, su tamaño es %f y nos vamos\n", nuevo_archivo->padre, nuevo_archivo->tamanio);

		list_add(archivos, nuevo_archivo);

		list_clean(bloques_pedidos_por_marta);

		char* mensaje = malloc(100);
		sprintf(mensaje, "Archivo %s agregado al filesystem en %s.\n", nuevo_archivo->nombre, concatenar_padres(sistema[padre]));
		log_info(logger, mensaje);
		free(mensaje);

		persistir_archivos();

	}

	free(ruta_archivo);
	free(nombre_nodo);
	free(nombre_archivo);

	free(datos_archivo);

}

bool es_copia_de_este_bloque(bloque_datos *bloque)
{
	return (bloque->num_bloque_archivo == bloque_global_a_buscar);
}

void *devolver_este_archivo(char *nombre)
{
	int i;
	for(i = 0; i < list_size(archivos); i++)
	{
		archivo *archivo_actual = list_get(archivos, i);

		if( string_starts_with(archivo_actual->nombre, nombre))
			return archivo_actual;
	}

	return 0;
}

void *devolver_este_archivo_con_padre(char *nombre, int padre)
{
	int i;
	for(i = 0; i < list_size(archivos); i++)
	{
		archivo *archivo_actual = list_get(archivos, i);

		if( string_starts_with(archivo_actual->nombre, nombre) && archivo_actual->padre == padre)
			return archivo_actual;
	}

	return 0;
}

int filesize(FILE *archivo)
{
	int prev = ftell(archivo);
	fseek(archivo, 0L, SEEK_END);
	int size = ftell(archivo);
	fseek(archivo, prev, SEEK_SET);

	return size;
}

int buscar_ultimo_enter(char *mmap, int cuanto)
{
	int i=0;
	int ultimo_enter;

	for (i=0; (i<cuanto); i++)
	{
		char c = mmap[i];

		if (c=='\n')
			ultimo_enter = i;
	}

	return ultimo_enter;
}

void llenar_barraceros(int ultimo_enter, char *mmap, int cuanto)
{
	int i = 0;
	for(i=(ultimo_enter + 1); i<cuanto; i++)
	{
		mmap[i] = '\0';
	}
}

char* guardar_offset(int tamanio_offset, char *mmap, int cuanto)
{
	char *datos_offset = malloc(tamanio_offset);

	int i = 0;
	for(i = 0; i<tamanio_offset; i++)		//creo que este for no es necesario
	{
		datos_offset[i] = '\0';
	}

	memcpy(datos_offset, mmap + cuanto - tamanio_offset, tamanio_offset);

	return datos_offset;
}

char *mmapear_blocksize(int fd_archivo, int cuanto, int desde)
{
	char *mmap_archivo = mmap( NULL, cuanto, PROT_READ | PROT_WRITE, MAP_SHARED, fd_archivo, BLOCKSIZE * desde );

	char *copia_mmap = malloc(cuanto);

	memcpy(copia_mmap, mmap_archivo, cuanto);

	munmap(mmap_archivo, cuanto);

	return copia_mmap;

}

t_list* partir(FILE *archivo)
{
	int fd_archivo = fileno(archivo);
	int tamanio_archivo = filesize(archivo);

	int redondear;
	int tamanio_en_bloques = 1;

	for(redondear = 1; tamanio_archivo > BLOCKSIZE*redondear; redondear++)
	{
		tamanio_en_bloques = redondear + 1;				//cuántos bloques voy a necesitar para el archivo
	}


	char *mmap;
	int ultimo_enter;

	int hubo_offset = 0;
	char *datos_offset;
	int tamanio_offset;

	char *datos_nuevo_offset;
	int tamanio_offset_a_desplazar;

	int loop = 1;

	int tamanio_a_leer;

	do
	{
		if (loop > 1) { free(mmap); }

		if (loop == tamanio_en_bloques)
		{
			tamanio_a_leer = tamanio_archivo - (tamanio_en_bloques - 1) * BLOCKSIZE;
			mmap = mmapear_blocksize(fd_archivo, tamanio_a_leer, loop - 1);

			mmap = realloc(mmap, BLOCKSIZE);

			ultimo_enter = buscar_ultimo_enter(mmap, tamanio_a_leer);

			int i = 0;
			for(i=(ultimo_enter + 1); i<BLOCKSIZE; i++)
			{
				mmap[i] = '\0';
			}

			tamanio_a_leer = BLOCKSIZE;

		}

		else
		{
			tamanio_a_leer = BLOCKSIZE;
			mmap = mmapear_blocksize(fd_archivo, tamanio_a_leer, loop - 1);

		}

		if (hubo_offset == 1 /* && ultimo_enter + tamanio_offset <= BLOCKSIZE*/)			//debo guardar lo que será desplazado al poner el offset anterior
		{
			tamanio_offset_a_desplazar = tamanio_offset;
			datos_nuevo_offset = guardar_offset(tamanio_offset_a_desplazar, mmap, tamanio_a_leer);

			char *mmap_con_offset = malloc(tamanio_a_leer + tamanio_offset + 1);

			int i = 0;
			for(i=0; i<tamanio_a_leer + tamanio_offset + 1; i++)
			{
				mmap_con_offset[i] = '\0';
			}

			memcpy(mmap_con_offset, datos_offset, tamanio_offset);
			memcpy(mmap_con_offset + tamanio_offset /*+ 1*/, mmap, tamanio_a_leer);

			free(mmap);
			mmap = malloc(tamanio_a_leer);
			memcpy(mmap, mmap_con_offset, tamanio_a_leer);
			free(mmap_con_offset);

			free(datos_offset);
			datos_offset = malloc(tamanio_offset_a_desplazar);
			memcpy(datos_offset, datos_nuevo_offset, tamanio_offset_a_desplazar);
			free(datos_nuevo_offset);

		}

		ultimo_enter = buscar_ultimo_enter(mmap, tamanio_a_leer);


		if ((ultimo_enter + 1) == tamanio_archivo)	//si el enter es el último caracter
			hubo_offset = 0;
		else										//si va a tener offset
		{

			if (hubo_offset == 0)		//si no hay que concatenar offset de antes...
			{
				tamanio_offset = tamanio_a_leer - ultimo_enter - 1;

				datos_offset = malloc(tamanio_offset);

				int i = 0;
				for(i = 0; i<tamanio_offset; i++)		//creo que este for no es necesario
				{
					datos_offset[i] = '\0';
				}

				memcpy(datos_offset, mmap + tamanio_a_leer - tamanio_offset, tamanio_offset);

				llenar_barraceros(ultimo_enter, mmap, tamanio_a_leer);
			}
			else		//en este caso datos_offset viene con info y hay que concatenarla con el nuevo offset
			{

				tamanio_offset = tamanio_a_leer - ultimo_enter - 1;

				char *datos_nuevo_offset = malloc(tamanio_offset);

				int i = 0;
				for(i = 0; i<tamanio_offset; i++)		//creo que este for no es necesario
				{
					datos_nuevo_offset[i] = '\0';
				}
				memcpy(datos_nuevo_offset, mmap + ultimo_enter + 1/*BLOCKSIZE - tamanio_offset*/, tamanio_offset);



				char *offset_final = malloc(tamanio_offset + tamanio_offset_a_desplazar);

				for(i = 0; i<tamanio_offset + tamanio_offset_a_desplazar; i++)		//creo que este for no es necesario
				{
					offset_final[i] = '\0';
				}

				memcpy(offset_final, datos_nuevo_offset, tamanio_offset);
				memcpy(offset_final + tamanio_offset, datos_offset, tamanio_offset_a_desplazar);

				free(datos_nuevo_offset);

				free(datos_offset);
				datos_offset = malloc(tamanio_offset + tamanio_offset_a_desplazar);

				memcpy(datos_offset, offset_final, tamanio_offset + tamanio_offset_a_desplazar);
				free(offset_final);

				tamanio_offset = tamanio_offset + tamanio_offset_a_desplazar;

				llenar_barraceros(ultimo_enter, mmap, tamanio_a_leer);
			}

			hubo_offset = 1;

		}

		int exito = triplicar_copia_nodos(mmap, loop, 0);

		if (exito == 0)
		{

			printf("El archivo no se puede distribuir entre los nodos disponibles.\n");

			eliminar_bloques_fallidos(bloques_copiandose);

			return list_create();
			break;
		}

		loop = loop + 1;


	} while(loop <= tamanio_en_bloques);

	free(mmap);
	if (hubo_offset == 1) free(datos_offset);

	return bloques_copiandose;
}

int triplicar_copia_nodos(char* datos, int numero_de_bloque_archivo, int motivo)
{
	t_list *nodos_a_copiar = list_create();
	nodos_a_copiar = tres_nodos_convenientes();

	if (list_size(nodos_a_copiar) < 3)
	{
		return 0;
	}

	nodo *primer_nodo = list_get(nodos_a_copiar, 0);
	nodo *segundo_nodo = list_get(nodos_a_copiar, 1);
	nodo *tercer_nodo = list_get(nodos_a_copiar, 2);

	list_destroy(nodos_a_copiar);

	int bloque_libre_nodo_uno  = buscar_bloque_libre(primer_nodo);
	int bloque_libre_nodo_dos  = buscar_bloque_libre(segundo_nodo);
	int bloque_libre_nodo_tres = buscar_bloque_libre(tercer_nodo);

	printf("\nEnviado bloque de archivo %d a:\n", numero_de_bloque_archivo);

	envio_bloque *copia_uno = malloc(sizeof(envio_bloque));
	copia_uno->num_copia = 1;
	copia_uno->bloque_archivo = numero_de_bloque_archivo;
	copia_uno->bloque_nodo = bloque_libre_nodo_uno;
	copia_uno->datos = datos;
	copia_uno->nodo = primer_nodo;
	copia_uno->motivo = motivo;
	pthread_create(&enviar_copia_uno, NULL, (void *) enviar_bloque, (void *) copia_uno);

	envio_bloque *copia_dos = malloc(sizeof(envio_bloque));
	copia_dos->num_copia = 2;
	copia_dos->bloque_archivo = numero_de_bloque_archivo;
	copia_dos->bloque_nodo = bloque_libre_nodo_dos;
	copia_dos->datos = datos;
	copia_dos->nodo = segundo_nodo;
	copia_dos->motivo = motivo;
	pthread_create(&enviar_copia_dos, NULL, (void *) enviar_bloque, (void *) copia_dos);

	envio_bloque *copia_tres = malloc(sizeof(envio_bloque));
	copia_tres->num_copia = 3;
	copia_tres->bloque_archivo = numero_de_bloque_archivo;
	copia_tres->bloque_nodo = bloque_libre_nodo_tres;
	copia_tres->datos = datos;
	copia_tres->nodo = tercer_nodo;
	copia_tres->motivo = motivo;
	pthread_create(&enviar_copia_tres, NULL, (void *) enviar_bloque, (void *) copia_tres);

	pthread_join(enviar_copia_uno, NULL);
	pthread_join(enviar_copia_dos, NULL);
	pthread_join(enviar_copia_tres, NULL);

	if (copia_uno->num_copia == 0 || copia_dos->num_copia == 0 || copia_tres->num_copia == 0)
	{
		return 0;
	}

	return 1;
}

void enviar_bloque(envio_bloque *copia)
{
	if (copia->nodo->estado == false)
	{
		copia->num_copia = 0;
		return;
	}

	uint32_t operacion = SETBLOQUE;
	uint32_t bloque_a_escribir = copia->bloque_nodo;
	char *datos = copia->datos;
	uint32_t size_total_para_enviar = sizeof(operacion) + sizeof(bloque_a_escribir) + BLOCKSIZE;

	char* paquete_serializado = malloc(size_total_para_enviar);

	int offset = 0;

	uint32_t size_to_send = sizeof(operacion);
	memcpy(paquete_serializado+offset, &operacion, size_to_send);
	offset += size_to_send;

	size_to_send = sizeof(bloque_a_escribir);
	memcpy(paquete_serializado+offset, &bloque_a_escribir, size_to_send);
	offset += size_to_send;

	size_to_send = BLOCKSIZE;
	memcpy(paquete_serializado+offset, datos, BLOCKSIZE);
	offset += size_to_send;

	int enviado = enviar_datos(copia->nodo->socket, paquete_serializado, size_total_para_enviar);

	free(paquete_serializado);

	if (enviado == -1)
	{
		printf("Error en %s\n", copia->nodo->nombre);
		printf("El nodo está desconectado o no disponible.\n");

		if(copia->num_copia == 1)
		{
			pthread_cancel(enviar_copia_dos);
			pthread_cancel(enviar_copia_tres);

		}
		else if(copia->num_copia == 2)
		{
			pthread_cancel(enviar_copia_uno);
			pthread_cancel(enviar_copia_tres);

		}
		else if(copia->num_copia == 3)
		{
			pthread_cancel(enviar_copia_uno);
			pthread_cancel(enviar_copia_dos);
		}

		copia->num_copia = 0;

		return;
	}

	pthread_cancel(copia->nodo->hilo);

	int exito;
	recv(copia->nodo->socket, &exito, sizeof(uint32_t), MSG_WAITALL);

	pthread_t hilo_mantener_nodo;
	pthread_create(&hilo_mantener_nodo, NULL, (void*) mantener_conexion_nodo, (void*) copia->nodo->socket);

	copia->nodo->hilo = hilo_mantener_nodo;

	if(exito == 21) //set_ok
	{
		printf("%s en bloque %d,\n",
		copia->nodo->nombre, copia->bloque_nodo);
		fflush(stdout);

		copia->nodo->espacio_utilizado += 20;

		bloque_datos *bloque = malloc(sizeof(bloque_datos));

		bloque->num_bloque_archivo = copia->bloque_archivo;
		bloque->nodo_padre = string_duplicate(copia->nodo->nombre);
		bloque->bloque_nodo_padre = copia->bloque_nodo;

		if (copia->motivo == 0)
			list_add(bloques_copiandose, bloque);

		if (copia->motivo == 1)
			list_add(bloques_pedidos_por_marta, bloque);


		list_add(bloques, bloque);
	}

}

int buscar_bloque_libre(nodo* nodo_a_buscar)
{

	nombre_global_a_buscar = nodo_a_buscar->nombre;

	int bloque_maximo = nodo_a_buscar->espacio_total / 20;

	t_list *bloques_de_este_nodo = list_filter(bloques, (void*) es_bloque_de_este_nodo);

	if(list_size(bloques_de_este_nodo) == 0) return 0;

	int i = 0;
	for(i = 0; i <= bloque_maximo; i++)
	{

		bloque_global_a_buscar = i;

		if (list_any_satisfy(bloques_de_este_nodo, (void*) esta_ocupado_el_bloque_global) == false) //si es false, es porque nadie lo tiene ocupado
		{
			list_destroy(bloques_de_este_nodo);
			return i;
		}
	}

	list_destroy(bloques_de_este_nodo);

	return -1;
}

void eliminar_bloques_fallidos(t_list *bloques_fallidos)
{
	int i = 0;
	for(i = 0; i < list_size(bloques_fallidos); i++)
	{
		bloque_datos *bloque_actual = list_get(bloques_fallidos, i);

		nombre_global_a_buscar = bloque_actual->nodo_padre;
		bloque_global_a_buscar = bloque_actual->bloque_nodo_padre;

		nodo *nodo_a_liberar = obtener_nodo_por_nombre(nombre_global_a_buscar);
		nodo_a_liberar->espacio_utilizado -= 20;

		list_remove_by_condition(bloques, (void*) es_este_bloque_segun_nodo);


	}
}

bool es_bloque_de_este_nodo(bloque_datos *bloque)
{
	return string_starts_with(bloque->nodo_padre, nombre_global_a_buscar);
}

bool esta_ocupado_el_bloque_global(bloque_datos *bloque)
{
	bool coincide_bloque = bloque->bloque_nodo_padre == bloque_global_a_buscar;

	return (coincide_bloque);
}

bool es_este_bloque_segun_nodo(bloque_datos *bloque)
{
	bool coincide_nombre	 = string_starts_with(bloque->nodo_padre, nombre_global_a_buscar);
	bool coincide_num_bloque = bloque->bloque_nodo_padre == bloque_global_a_buscar;

	return (coincide_nombre && coincide_num_bloque);
}

bool es_este_bloque_segun_archivo(bloque_datos *bloque)
{
	bool coincide_nombre	 = string_starts_with(bloque->nodo_padre, nombre_global_a_buscar);
	bool coincide_num_bloque = bloque->num_bloque_archivo == bloque_global_a_buscar;

	return (coincide_nombre && coincide_num_bloque);
}

void destructor_bloque(bloque_datos *bloque)
{
	free(bloque->nodo_padre);
	free(bloque);
}

t_list *tres_nodos_convenientes()	//copia la lista de nodos, quita los llenos y desconectados, los ordena por menos ocupados y te los devuelve.
{
	t_list *duplicado_nodos = list_create();

	list_add_all(duplicado_nodos, nodos);

	int i;
	for(i = 0; i < list_size(duplicado_nodos); i++)
	{
		list_remove_by_condition(duplicado_nodos, (void*) nodo_esta_lleno);
		list_remove_by_condition(duplicado_nodos, (void*) nodo_esta_desconectado);
	}

	list_sort(duplicado_nodos, (void*) ordenar_por_espacio_utilizado);

	int cantidad_nodos_disponible = list_size(duplicado_nodos);
	t_list *nodos_convenientes;

	if (cantidad_nodos_disponible < 3)
		nodos_convenientes = list_take(duplicado_nodos, cantidad_nodos_disponible);
	else
		nodos_convenientes = list_take(duplicado_nodos, 3);

	list_destroy(duplicado_nodos);

	return nodos_convenientes;
}

bool ordenar_por_espacio_utilizado(nodo* nodo_ocupado, nodo* nodo_mas_ocupado)
{
	int capacidad_nodo_ocupado = nodo_ocupado->espacio_utilizado;
	int capacidad_nodo_mas_ocupado = nodo_mas_ocupado->espacio_utilizado;

	return (capacidad_nodo_ocupado < capacidad_nodo_mas_ocupado);
}

bool ordenar_por_tamanio(nodo* nodo_grande, nodo* nodo_mas_grande)
{
	int tamanio_nodo_grande = nodo_grande->espacio_total;
	int tamanio_nodo_mas_grande = nodo_mas_grande->espacio_total;

	return (tamanio_nodo_grande < tamanio_nodo_mas_grande);
}

nodo *obtener_nodo_por_nombre(char *nombre)
{
	nombre_global_a_buscar = nombre;
	nodo *nodo_encontrado = list_find(nodos, (void *) buscar_nodo_por_nombre);

	return nodo_encontrado;
}

bool nodo_esta_lleno(nodo* nodo)
{
	return (nodo->espacio_utilizado == nodo->espacio_total);
}

bool nodo_esta_desconectado(nodo* nodo)
{
	return (nodo->estado == false);
}

void conectar_nodos(int socket_servidor)
{
	int cantidad_conectados = 0;
	listen(socket_servidor, 10);

	do
	{
		printf("Intentando conectar un nodo... \n");

		int socket_nodo_conectado = 0;

		do
		{	socket_nodo_conectado = agregar_nodo(socket_servidor); }
		while(socket_nodo_conectado == 0);

		cantidad_conectados = list_count_satisfying(nodos, (void*) esta_conectado);
		printf("Nodos conectados totales: %d\n", cantidad_conectados);

	}while(cantidad_conectados < CANTIDAD_NODOS);

	char* mensaje = malloc(100);
	sprintf(mensaje, "Conexión inicial de nodos completa.\n");
	log_info(logger, mensaje);
	free(mensaje);

}

int agregar_nodo(int socket_servidor)
{
	int socket_cliente_nodo = aceptarConexion(socket_servidor);

	if (socket_cliente_nodo > 0)
	{
		nodo* nuevo_nodo = guardar_informacion_nodo(socket_cliente_nodo);

		pthread_t hilo_mantener_nodo;
		pthread_create(&hilo_mantener_nodo, NULL, (void*) mantener_conexion_nodo, (void*) socket_cliente_nodo);

		nuevo_nodo->hilo = hilo_mantener_nodo;

		persistir_nodos();
	}

	return socket_cliente_nodo;

}

void mantener_conexion_nodo(int socket_nodo_conectado)
{
    int operacion;

	if (recv(socket_nodo_conectado, &operacion, sizeof(uint32_t), MSG_PEEK) == 0 )
    {
		pthread_mutex_lock( &mutex_socket_global );
		socket_global_a_buscar = socket_nodo_conectado;
		nodo* nodo_buscado = list_find(nodos, (void*) buscar_nodo_por_socket);

		if (nodo_buscado != NULL)
		{
			nodo_buscado->socket = 0;
			nodo_buscado->estado = false;
		}

		close(socket_nodo_conectado);

		pthread_mutex_unlock( &mutex_socket_global );

		chequear_disponibilidad_archivos();

    }


}

int buscar_nodo_por_socket(nodo *nodo)
{
	if (nodo->socket == socket_global_a_buscar)
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "%s desconectado.\n", nodo->nombre);
		log_info(logger, mensaje);
		free(mensaje);
		return 1;
	}
	return 0;
}

int buscar_nodo_por_nombre(nodo *nodo)
{
	if (string_starts_with(nodo->nombre, nombre_global_a_buscar))
	{
		return 1;
	}
	return 0;
}

bool coincide_nodo_con_nombre(nodo *nodo)
{
	return string_starts_with(nodo->nombre, nombre_global_a_buscar);
}

void mostrar_nodos()
{
	printf("\nNodos en sistema:\n");
	list_iterate(nodos, (void*) mostrar_nodo);

	int i = 0;
	int utilizado_acumulado = 0;
	int total_acumulado = 0;

	for(i = 0; i < list_size(nodos); i++)
	{
		nodo *nodo_actual = list_get(nodos, i);
		utilizado_acumulado += nodo_actual->espacio_utilizado;
		total_acumulado += nodo_actual->espacio_total;
	}

	char* mensaje = malloc(100);
	sprintf(mensaje, "Espacio total: %dMB utilizados de %dMB.\n.", utilizado_acumulado, total_acumulado);
	log_info(logger, mensaje);
	free(mensaje);
}

void mostrar_nodo(nodo* nodo)
{
	printf("%s\n", nodo->nombre);
	printf("IP:                 %s\n", nodo->ip);
	printf("Puerto:             %s\n", nodo->puerto);
	printf("Espacio total:      %d\n", nodo->espacio_total);
	printf("Espacio disponible: %d\n", (nodo->espacio_total - nodo->espacio_utilizado));
	if (nodo->estado == true)
		printf("Estado:             Conectado\n");
	else
		printf("Estado:             Desconectado\n");

	printf("\n");

}


nodo* guardar_informacion_nodo(int socket_nodo)
{
	uint32_t operacion = 0;
	recv(socket_nodo, &operacion, sizeof(uint32_t), 0);

	uint32_t largo_nombre = 0;
	recv(socket_nodo, &largo_nombre, sizeof(uint32_t), 0);

	char *nombre_nodo = malloc(largo_nombre);
	recv(socket_nodo, nombre_nodo, largo_nombre, 0);

	uint32_t largo_ip = 0;
	recv(socket_nodo, &largo_ip, sizeof(uint32_t), 0);

	char *ip = malloc(largo_ip);
	recv(socket_nodo, ip, largo_ip, 0);

	uint32_t largo_puerto = 0;
	recv(socket_nodo, &largo_puerto, sizeof(uint32_t), 0);

	char *puerto = malloc(largo_puerto);
	recv(socket_nodo, puerto, largo_puerto, 0);

	uint32_t tamanio_mb = 0;
	recv(socket_nodo, &tamanio_mb, sizeof(uint32_t), 0);

	nombre_global_a_buscar = nombre_nodo;

	if (list_find(nodos, (void*) buscar_nodo_por_nombre) == 0)
	{
		nodo *nuevo_nodo = malloc(sizeof(nodo));

		nuevo_nodo->nombre = nombre_nodo;
		nuevo_nodo->ip = ip;
		nuevo_nodo->puerto = puerto;
		nuevo_nodo->socket = socket_nodo;
		nuevo_nodo->espacio_total = tamanio_mb;
		nuevo_nodo->espacio_utilizado = 0;
		nuevo_nodo->estado = true;

		list_add(nodos, nuevo_nodo);
		list_add(nodos_para_marta, nuevo_nodo);

		char* mensaje = malloc(100);
		sprintf(mensaje, "%s conectado.\n", nuevo_nodo->nombre);
		log_info(logger, mensaje);
		free(mensaje);

		return nuevo_nodo;

	}
	else
	{
		nodo* nodo_existente = list_find(nodos, (void*) buscar_nodo_por_nombre);
		char* mensaje = malloc(100);
		sprintf(mensaje, "%s reconectado.\n", nodo_existente->nombre);
		nodo_existente->estado = true;
		nodo_existente->socket = socket_nodo;
		log_info(logger, mensaje);
		free(mensaje);

		chequear_disponibilidad_archivos();

		free(ip);
		free(puerto);

		return nodo_existente;

	}

}

bool esta_conectado(nodo *nodo)
{
	return nodo->estado;
}

char* eliminar_nodo(char *nombre_nodo)
{
	nombre_global_a_buscar = nombre_nodo;

	char *nombre_real;

	if (list_find(nodos, (void*) buscar_nodo_por_nombre) != 0)
	{
		nodo* nodo_existente = list_find(nodos, (void*) buscar_nodo_por_nombre);

		nombre_real = string_duplicate(nodo_existente->nombre);

		nombre_global_a_buscar = nombre_real;


		if(nodo_existente->estado == true)
		{
			pthread_cancel(nodo_existente->hilo);
			close(nodo_existente->socket);
		}


		list_remove_and_destroy_by_condition(nodos, (void*) coincide_nodo_con_nombre, (void*) destructor_nodo);

		persistir_nodos();
		persistir_archivos();

		int i = 0;
		for(i = 0; i < list_size(bloques); i++)
		{
			list_remove_by_condition(bloques, (void*) es_bloque_de_este_nodo);	//primero sólo los remuevo
		}

		for(i = 0; i < list_size(archivos); i++)
		{
			archivo *archivo_actual = list_get(archivos, i);

			int sigue_iterando = 1;
			while(sigue_iterando)
			{
				int tamanio_lista = list_size(archivo_actual->lista_bloques_datos);
				list_remove_and_destroy_by_condition(archivo_actual->lista_bloques_datos, (void*) es_bloque_de_este_nodo, (void*) destructor_bloque);

				if (tamanio_lista == list_size(archivo_actual->lista_bloques_datos)) //es decir que ya no hay nada para quitar
					sigue_iterando = 0;
			}
		}
	}

	chequear_disponibilidad_archivos();

	return nombre_real;

}
void destructor_nodo(nodo *nodo)
{
	free(nodo->nombre);
	free(nodo->ip);
	free(nodo->puerto);
	free(nodo);
}

char* concatenar_padres(un_directorio *directorio)
{
	char *direccion_anterior = "";
	char *direccion;

	direccion = string_duplicate(directorio->directorio);

	un_directorio *directorio_actual = directorio;

	if (directorio->posicion == 0)
	{
		return direccion;
		free(direccion);
		free(direccion_anterior);
	}

	char *direccion_con_raiz = string_new();

	if(directorio_actual->padre == 0)
	{
		string_append(&direccion_con_raiz, "/");
		string_append(&direccion_con_raiz, direccion);

		free(direccion);

		return direccion_con_raiz;
	}

	int primera_vez = 0;
	while(directorio_actual->padre != 0)
	{
		free(direccion);
		directorio_actual = sistema[directorio_actual->padre];
		direccion = string_duplicate(directorio_actual->directorio);
		string_append(&direccion, "/");
		string_append(&direccion, direccion_anterior);
		if (primera_vez == 0) primera_vez = 1; else free(direccion_anterior); //para leakear 0 bytes
		direccion_anterior = string_duplicate(direccion);
	}

	string_append(&direccion_con_raiz, "/");
	string_append(&direccion_con_raiz, direccion);
	string_append(&direccion_con_raiz, directorio->directorio);

	free(direccion);
	free(direccion_anterior);

	return direccion_con_raiz;
}

int consola()
{
	un_directorio *ultimo_directorio = sistema[0];

	un_directorio *directorio_a_mover;
	int moviendo_directorio = 0;
	int padre_directorio_lider = 0;

	char *archivo_a_mover;
	int moviendo_archivo = 0;
	int padre_archivo_lider = 0;

	int opcion = 1;

	while (opcion != '0')
	{
		char *concatenacion_padres = concatenar_padres(ultimo_directorio);
		printf("\nDirectorio actual: %s\n", concatenacion_padres);

		free(concatenacion_padres);

		//mostrar_sistema();

		char input[100];

		fgets(input, 100, stdin);

		int i;
		int enter;
		for (i = 0; i < 100; i++)
		{
			if (input[i] == '\n')
			{
				enter = i;
				break;
			}
		}

		for (i = enter; i < 100; i++)
		{
			input[i] = '\0';
		}

		if (input[0] == 'l' && input[1] == 's')
		{
			chequear_disponibilidad_archivos();
			mostrar_hijos_de(ultimo_directorio->posicion);

		}

		if (input[0] == 'c' && input[1] == 'd' && input[2] == ' ')
		{
			if (input[3] == '.' && input[4] == '.')
				ultimo_directorio = sistema[ultimo_directorio->padre];
			else
			{
				char *hijo = malloc(enter+1-3);
				memcpy(hijo, input + 3, enter+1-3);

				int posicion_directorio = buscar_hijo_de(ultimo_directorio->posicion, hijo);

				if (posicion_directorio != 1025)
					ultimo_directorio = sistema[posicion_directorio];

				free(hijo);

			}
		}

		if (input[0] == 'd' && input[1] == 'i' && input[2] == 'r')
		{
			printf("1 - Crear directorio aquí\n");
			printf("2 - Eliminar este directorio\n");
			printf("3 - Renombrar este directorio\n");

			if (moviendo_directorio == 0)
				printf("4 - Mover este directorio\n");
			else
				printf("4 - Mover directorio %s aquí\n", directorio_a_mover->directorio);

			int car = getchar();

			if (car != '\n')
			{
				getchar();

				switch(car)
				{
					case '1':
					{
						char input_directorio_crear[100];
						printf("Nombre de nuevo directorio: ");
						fgets(input_directorio_crear, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_directorio_crear[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_directorio_crear[i] = '\0';
						}

						if (directorio_tiene_nombre_legal(input_directorio_crear) == 0)
						{
							printf("%s no es un nombre válido.\n", input_directorio_crear);
							break;
						}

						char *nombre_directorio = malloc(enter+1);
						memcpy(nombre_directorio, input_directorio_crear, enter+1);	//exactamente el nombre y un \0

						ultimo_directorio = crear_directorio(nombre_directorio, ultimo_directorio->posicion);

						free(nombre_directorio);

						break;
					}
					//no break

					case '2':
					{
						if (ultimo_directorio->posicion == 0)
						{
							printf("No se puede borrar el directorio raíz. ¿Quizá quieras formatear?\n");
							break;
						}

						do
						{
							printf("Confirme eliminar el directorio %s y todos sus contenidos, aquí no hay papelera de reciclaje. (y/n): ", ultimo_directorio->directorio);
							car = getchar();
							getchar();
							printf("\n");

						} while(car != 'y' && car != 'n');

						if (car == 'y')
						{
							un_directorio *directorio_a_borrar = ultimo_directorio;
							ultimo_directorio = sistema[ultimo_directorio->padre];
							eliminar_directorio(directorio_a_borrar);

						}

						break;
					}
					//no break
					
					case '3':
					{
						if (ultimo_directorio->posicion == 0)
						{
							printf("No se puede renombrar el directorio raíz.\n");
							break;
						}

						char input_directorio_renombrar[100];
						printf("Nuevo nombre de directorio: ");
						fgets(input_directorio_renombrar, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_directorio_renombrar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_directorio_renombrar[i] = '\0';
						}

						if (directorio_tiene_nombre_legal(input_directorio_renombrar) == 0)
						{
							printf("%s no es un nombre válido.\n", input_directorio_renombrar);
							break;
						}

						char *nombre_directorio = malloc(enter+1);
						memcpy(nombre_directorio, input_directorio_renombrar, enter+1);	//exactamente el nombre y un \0

						renombrar_directorio(ultimo_directorio, nombre_directorio);

						free(nombre_directorio);

						break;
					}
					//no break
					
					case '4':
					{
						if (ultimo_directorio->posicion == 0)
						{
							printf("No se puede mover el directorio raíz.\n");
							break;
						}

						if (moviendo_directorio == 0)
						{
							moviendo_directorio = 1;
							printf("Navegue hasta el directorio donde quiere mover este directorio.\n");
							directorio_a_mover = ultimo_directorio;
							padre_directorio_lider = directorio_a_mover->posicion;

							ultimo_directorio = sistema[directorio_a_mover->padre];
						}
						else
						{
							if (es_subdirectorio(ultimo_directorio, padre_directorio_lider) == 1)
							{
								printf("No se puede mover el directorio a un subdirectorio.\n");
								break;
							}

							moviendo_directorio = 0;
							mover_directorio(directorio_a_mover, ultimo_directorio->posicion);
							ultimo_directorio = directorio_a_mover;
						}
						break;
					}
					//no break

					default: break;
				}
			}
		}

		if (input[0] == 'n' && input[1] == 'o' && input[2] == 'd')
		{
			printf("1 - Mostrar nodos\n");
			printf("2 - Conectar nodo\n");
			printf("3 - Eliminar nodo\n");
			printf("4 - Desconectar nodo\n");

			int car = getchar();

			if (car != '\n')
			{
				getchar();

				switch(car)
				{
					case '1':
					{
						mostrar_nodos();

						break;
					}
					//no break

					case '2':
					{
						printf("Esperando un nodo...\n");
						agregar_nodo(socket_servidor);
						printf("Nodos totales: %d", list_size(nodos));

						break;
					}
					//no break

					case '3':
					{
						char nodo_a_eliminar[100];
						printf("Nodo a eliminar: ");
						fgets(nodo_a_eliminar, 100, stdin);

						enter = 99;

						for (i = 0; i < 100; i++)
						{
							if (nodo_a_eliminar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							nodo_a_eliminar[i] = '\0';
						}

						nombre_global_a_buscar = nodo_a_eliminar;
						if (list_find(nodos, (void*) buscar_nodo_por_nombre) == 0)
						{
							printf("No existe un nodo con ese nombre.\n");
							break;
						}

						char *nodo_eliminado = eliminar_nodo(nodo_a_eliminar);
						printf("Nodo %s eliminado.", nodo_eliminado);

						free(nodo_eliminado);

						break;
					}
					//no break

					case '4':
					{
						char nodo_a_desconectar[100];
						printf("Nodo a desconectar: ");
						fgets(nodo_a_desconectar, 100, stdin);

						enter = 99;

						for (i = 0; i < 100; i++)
						{
							if (nodo_a_desconectar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							nodo_a_desconectar[i] = '\0';
						}

						nombre_global_a_buscar = nodo_a_desconectar;
						if (list_find(nodos, (void*) buscar_nodo_por_nombre) == 0)
						{
							printf("No existe un nodo con ese nombre.\n");
							break;
						}

						nodo *nodo = list_find(nodos, (void *) buscar_nodo_por_nombre);
						pthread_cancel(nodo->hilo);
						printf("Nodo %s desconectado.", nodo->nombre);

						close(nodo->socket);
						nodo->estado = false;

						break;

					}
					//no break
				}
			}
		}

		if (input[0] == 'a' && input[1] == 'r' && input[2] == 'c')
		{
			chequear_disponibilidad_archivos();

			printf("1 - Agregar archivo en %s\n", ultimo_directorio->directorio);
			printf("2 - Copiar un archivo al local\n");
			if (moviendo_archivo == 0)
				printf("3 - Mover archivo a otro directorio\n");
			else
				printf("3 - Mover %s aquí\n", archivo_a_mover);
			printf("4 - Eliminar archivo\n");
			printf("5 - Renombrar archivo\n");
			printf("6 - Solicitar MD5 de archivo\n");
			printf("7 - Ver distribución de bloques\n");
			printf("8 - Borrar bloque de archivo\n");
			printf("9 - Copiar bloque de archivo\n");

			int car = getchar();

			if (car != '\n')
			{
				getchar();

				switch(car)
				{
					case '1':
					{
						char ruta_archivo_a_agregar[100];
						printf("Ruta relativa al filesystem de archivo a agregar: ");
						fgets(ruta_archivo_a_agregar, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (ruta_archivo_a_agregar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							ruta_archivo_a_agregar[i] = '\0';
						}

						char* nombre_archivo = string_duplicate(obtener_archivo_de_ruta(ruta_archivo_a_agregar, enter));

						padre_global_a_buscar = ultimo_directorio->posicion;
						if (archivo_tiene_nombre_legal(nombre_archivo) == 0)
						{
							printf("%s no es un nombre válido o ya existe un archivo con ese nombre.\n", nombre_archivo);
							break;
						}

						agregar_archivo(ruta_archivo_a_agregar/*nombre_archivo*/, ultimo_directorio->posicion, nombre_archivo);

						persistir_archivos();
						persistir_nodos();

						free(nombre_archivo);

						break;
					}
					//no break

					case '2':
					{
						char archivo_a_copiar[100];
						printf("Archivo a copiar: ");
						fgets(archivo_a_copiar, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (archivo_a_copiar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							archivo_a_copiar[i] = '\0';
						}

						padre_global_a_buscar = ultimo_directorio->posicion;
						if (existe_archivo(archivo_a_copiar) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							break;
						}

						lectura_archivo *copia = malloc(sizeof(lectura_archivo));
						copia->nombre_archivo = string_duplicate(archivo_a_copiar);
						copia->padre = ultimo_directorio->posicion;
						copia->motivo = string_duplicate("copia-");

						char* mensaje = malloc(100);
						sprintf(mensaje, "Solicitada copia de archivo %s.\n", copia->nombre_archivo);
						log_info(logger, mensaje);
						free(mensaje);

						pthread_t hilo_copia;
						pthread_create(&hilo_copia, NULL, (void*) copiar_archivo_al_local, (void*) copia);

						break;
					}
					//no break

					case '3':
					{

						if (moviendo_archivo == 0)
						{
							char input_archivo_a_mover[100];
							printf("Archivo a mover: ");
							fgets(input_archivo_a_mover, 100, stdin);

							enter = 99;
							for (i = 0; i < 100; i++)
							{
								if (input_archivo_a_mover[i] == '\n')
								{
									enter = i;
									break;
								}
							}

							if (enter == 0) break;

							for (i = enter; i <= 100; i++)
							{
								input_archivo_a_mover[i] = '\0';
							}

							char *nombre_archivo_mover = malloc(enter+1);
							memcpy(nombre_archivo_mover, input_archivo_a_mover, enter+1);

							padre_global_a_buscar = ultimo_directorio->posicion;
							if ( devolver_este_archivo_con_padre(nombre_archivo_mover, ultimo_directorio->posicion) == NULL)
							{
								printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
								free(nombre_archivo_mover);
								break;
							}

								moviendo_archivo = 1;
								printf("Navegue hasta el directorio donde quiere mover este archivo.\n");
								archivo_a_mover = nombre_archivo_mover;
								padre_archivo_lider = ultimo_directorio->posicion;

								ultimo_directorio = sistema[ultimo_directorio->padre];

								free(nombre_archivo_mover);
						}

						else
						{
							moviendo_archivo = 0;
							mover_archivo(archivo_a_mover, padre_archivo_lider, ultimo_directorio->posicion);

							printf("%s movido a %s.\n", archivo_a_mover, ultimo_directorio->directorio);

							persistir_archivos();
						}



						break;

					}
					//no break

					case '4':
					{
						char archivo_a_eliminar[100];
						printf("Archivo a eliminar: ");
						fgets(archivo_a_eliminar, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (archivo_a_eliminar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							archivo_a_eliminar[i] = '\0';
						}

						padre_global_a_buscar = ultimo_directorio->posicion;
						if (existe_archivo(archivo_a_eliminar) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							break;
						}

						eliminar_archivo(archivo_a_eliminar, ultimo_directorio->posicion);
						printf("Archivo eliminado.\n");
						persistir_archivos();
						persistir_nodos();

						break;

					}
					//no break

					case '5':
					{
						char input_archivo_renombrar[100];
						printf("Archivo a renombrar: ");
						fgets(input_archivo_renombrar, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_archivo_renombrar[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_archivo_renombrar[i] = '\0';
						}

						char *nombre_archivo_renombrar = malloc(enter+1);
						memcpy(nombre_archivo_renombrar, input_archivo_renombrar, enter+1);

						padre_global_a_buscar = ultimo_directorio->posicion;

						if (existe_archivo(nombre_archivo_renombrar) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							break;
						}

						char input_nuevo_nombre[100];
						printf("Nuevo nombre de archivo: ");
						fgets(input_nuevo_nombre, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_nuevo_nombre[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_nuevo_nombre[i] = '\0';
						}

						char *nombre_archivo_nuevo = malloc(enter+1);
						memcpy(nombre_archivo_nuevo, input_nuevo_nombre, enter+1);

						renombrar_archivo(nombre_archivo_renombrar, ultimo_directorio->posicion,
										  nombre_archivo_nuevo);

						free(nombre_archivo_renombrar);
						free(nombre_archivo_nuevo);

						printf("Archivo renombrado.\n");
						persistir_archivos();

						break;

					}
					//no break

					case '6':
					{
						char input_archivo_md5[100];
						printf("Archivo a obtener MD5: ");
						fgets(input_archivo_md5, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_archivo_md5[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_archivo_md5[i] = '\0';
						}

						char *nombre_archivo_md5 = malloc(enter+1);
						memcpy(nombre_archivo_md5, input_archivo_md5, enter+1);

						pthread_mutex_lock( &mutex_padre_global );

						padre_global_a_buscar = ultimo_directorio->posicion;

						if (existe_archivo(nombre_archivo_md5) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							free(nombre_archivo_md5);
							pthread_mutex_unlock ( &mutex_padre_global );
							break;
						}

						pthread_mutex_unlock ( &mutex_padre_global );

						lectura_archivo *md5_struct = malloc(sizeof(lectura_archivo));
						md5_struct->nombre_archivo = string_duplicate(nombre_archivo_md5);
						md5_struct->padre = ultimo_directorio->posicion;
						md5_struct->motivo = string_duplicate("MD5-");


						pthread_t hilo_md5;
						pthread_create(&hilo_md5, NULL, (void*) obtener_md5, (void*) md5_struct);

						free(nombre_archivo_md5);

					//	int exito = obtener_md5(nombre_archivo_md5, ultimo_directorio->posicion);

					//	if (exito == 0)
					//		printf("No se puede obtener el MD5 de %s.\n", nombre_archivo_md5);


						break;

					}
					//no break

					case '7':
					{
						if (list_size(archivos) == 0)
						{
							printf("No hay archivos en sistema.");
							break;
						}

						printf("Distribución en sistema:\n");

						mostrar_informacion_archivos();

						break;
					}
					//no break

					case '8':
					{
						char input_archivo[100];
						printf("Archivo: ");
						fgets(input_archivo, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_archivo[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_archivo[i] = '\0';
						}

						char *nombre_archivo = malloc(enter+1);
						memcpy(nombre_archivo, input_archivo, enter+1);

						padre_global_a_buscar = ultimo_directorio->posicion;

						if (existe_archivo(nombre_archivo) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							free(nombre_archivo);
							break;
						}



						char input_bloque[100];
						printf("Bloque: ");
						fgets(input_bloque, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_bloque[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_bloque[i] = '\0';
						}

						char *bloque_texto = malloc(enter+1);
						memcpy(bloque_texto, input_bloque, enter+1);

						int num_bloque = strtol(bloque_texto, NULL, 10);



						char input_nodo[100];
						printf("En nodo: ");
						fgets(input_nodo, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_nodo[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_nodo[i] = '\0';
						}

						char *nombre_nodo = malloc(enter+1);
						memcpy(nombre_nodo, input_nodo, enter+1);



						if ( tiene_este_bloque(nombre_nodo, nombre_archivo, ultimo_directorio->posicion, num_bloque) == 0)
						{
							printf("El nodo %s no posee una copia del bloque %d de %s o no existe.\n",
									nombre_nodo, num_bloque, nombre_archivo);
							free(nombre_nodo);
							break;
						}

						nodo* nodo = obtener_nodo_por_nombre(nombre_nodo);


						padre_global_a_buscar = ultimo_directorio->posicion;
						borrar_este_bloque(num_bloque, nombre_archivo, nodo);

						printf("Bloque %d de %s borrado de %s.\n", num_bloque, nombre_archivo, nombre_nodo);

						persistir_archivos();
						persistir_nodos();

						free(bloque_texto);
						free(nombre_archivo);
						free(nombre_nodo);

						break;
					}
					//no break

					case '9':
					{
						char input_archivo[100];
						printf("Archivo: ");
						fgets(input_archivo, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_archivo[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_archivo[i] = '\0';
						}

						char *nombre_archivo = malloc(enter+1);
						memcpy(nombre_archivo, input_archivo, enter+1);

						padre_global_a_buscar = ultimo_directorio->posicion;

						if (existe_archivo(nombre_archivo) == 0)
						{
							printf("No existe un archivo con ese nombre en %s.\n", ultimo_directorio->directorio);
							free(nombre_archivo);
							break;
						}



						char input_bloque[100];
						printf("Bloque: ");
						fgets(input_bloque, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_bloque[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_bloque[i] = '\0';
						}

						char *bloque_texto = malloc(enter+1);
						memcpy(bloque_texto, input_bloque, enter+1);

						int num_bloque = strtol(bloque_texto, NULL, 10);


						archivo *archivo_a_copiar = devolver_este_archivo_con_padre(nombre_archivo, ultimo_directorio->posicion);

						if (num_bloque < 1 || num_bloque > cuantos_bloques_tiene(archivo_a_copiar))
						{
							printf("%d no es un bloque válido para %s.\n", num_bloque, archivo_a_copiar->nombre);
							break;
						}


						char input_nodo[100];
						printf("A nodo: ");
						fgets(input_nodo, 100, stdin);

						enter = 99;
						for (i = 0; i < 100; i++)
						{
							if (input_nodo[i] == '\n')
							{
								enter = i;
								break;
							}
						}

						if (enter == 0) break;

						for (i = enter; i <= 100; i++)
						{
							input_nodo[i] = '\0';
						}

						char *nombre_nodo = malloc(enter+1);
						memcpy(nombre_nodo, input_nodo, enter+1);


						int exito = copiar_este_bloque(nombre_nodo, nombre_archivo,ultimo_directorio->posicion, num_bloque);

						if (exito == -1)
						{
							printf("El nodo %s no posee espacio disponible, no está conectado, o no se puede obtener una copia del bloque.\n", nombre_nodo);
						}
						else
						{
							printf("Bloque %d de %s copiado a %s.\n", num_bloque, nombre_archivo, nombre_nodo);
							persistir_archivos();
							persistir_nodos();
						}

						free(bloque_texto);
						free(nombre_archivo);
						free(nombre_nodo);

						break;

					}
					//no break
				}
			}
		}


		if (input[0] == 'o' && input[1] == 'p')
		{
			printf("1 - Formatear filesystem\n");
			printf("2 - Salir\n");

			int car = getchar();

			if (car != '\n')
			{
				getchar();

				switch(car)
				{
					case '1':
					{
						do
						{
							printf("Confirme formatear el filesystem; se perderán TODOS sus contenidos, incluyendo directorios y archivos. (y/n): ");
							car = getchar();
							getchar();
							printf("\n");

						} while(car != 'y' && car != 'n');

						if (car == 'y')
						{
							do
							{
								printf("¿SEGURO desea formatear el filesystem? (y/n): ");
								car = getchar();
								getchar();
								printf("\n");
							} while(car != 'y' && car != 'n');

							if (car == 'y')
							{
								formatear();
								printf("Filesystem formateado.");
								ultimo_directorio = sistema[0];
							}
						}

						break;
					}
					//no break

					case '2':
					{
						do
						{
							printf("¿Desea salir? La estructuras serán guardadas. (y/n): ");
							car = getchar();
							getchar();
							printf("\n");

						} while(car != 'y' && car != 'n');

						if (car == 'y')
						{
							return 1;
						}

						break;
					}
				}
			}
		}
	}

	return 0;
}

int tiene_este_bloque(char *nombre_nodo, char *nombre_archivo, int padre, int num_bloque)
{

	archivo *archivo = devolver_este_archivo_con_padre(nombre_archivo, padre);

	int i;
	for (i = 0; i < list_size(archivo->lista_bloques_datos); i++)
	{
		bloque_datos *bloque_actual = list_get(archivo->lista_bloques_datos, i);

		if (bloque_actual->num_bloque_archivo == num_bloque &&
			string_starts_with(nombre_nodo, bloque_actual->nodo_padre))
		{
			return 1;
			break;
		}
	}

	return 0;
}

int copiar_archivo_al_local(lectura_archivo *copia)
{

	pthread_mutex_lock( &mutex_padre_global );
	pthread_mutex_lock( &mutex_nombre_global );
	padre_global_a_buscar = copia->padre;
	nombre_global_a_buscar = copia->nombre_archivo;
	archivo *archivo = devolver_este_archivo_con_padre(copia->nombre_archivo, copia->padre);
	pthread_mutex_unlock( &mutex_nombre_global );
	pthread_mutex_unlock( &mutex_padre_global );

	int cantidad_de_bloques = cuantos_bloques_tiene(archivo);

	char *nombre_con_fin = string_duplicate(copia->motivo);
	string_append(&nombre_con_fin, archivo->nombre);

	FILE *temporal;
	temporal = fopen(nombre_con_fin, "w");


	t_list *nodos_pedidos = list_create();

	int i;
	int cantidad_de_veces_minima = 0;
//	int nueva_cantidad_maxima = 0;
	for (i = 1; i <= cantidad_de_bloques; i++)
	{

		pthread_mutex_lock( &mutex_bloque_global );
		bloque_global_a_buscar = i;
		t_list *copias_de_este_bloque = list_filter(archivo->lista_bloques_datos, (void *) es_copia_de_este_bloque);
		pthread_mutex_unlock( &mutex_bloque_global );

		int j;

		nodo *nodo_que_menos_estuvo;
		int bloque_a_pedir = 0;

		int nodos_desconectados = 0;

		for (j = 0; j < list_size(copias_de_este_bloque); j++)
		{
			bloque_datos *bloque_actual = list_get(copias_de_este_bloque, j);

			pthread_mutex_lock( &mutex_nombre_global );
			//nombre_global_a_buscar = bloque_actual->nodo_padre;
			nodo *nodo_actual = obtener_nodo_por_nombre(bloque_actual->nodo_padre);
			pthread_mutex_unlock( &mutex_nombre_global );


			if ( esta_conectado(nodo_actual) )
			{
				int k;
				int cantidad_de_veces_actual = 0;

				for (k = 0; k < list_size(nodos_pedidos); k++)
				{
					if (list_get(nodos_pedidos, k) == nodo_actual)
					{
						cantidad_de_veces_actual++;
					}
				}

				if (cantidad_de_veces_actual <= cantidad_de_veces_minima)
				{
					nodo_que_menos_estuvo = nodo_actual;
					bloque_a_pedir = bloque_actual->bloque_nodo_padre;
				}
				else
				{
					cantidad_de_veces_minima = cantidad_de_veces_actual;
				}

			}
			else
			{
				nodos_desconectados++;
			}
		}

		if (nodos_desconectados == list_size(copias_de_este_bloque))
		{
			list_destroy(nodos_pedidos);

			if (string_starts_with(copia->motivo, "copia-"))
			{
				char* mensaje = malloc(100);
				sprintf(mensaje, "Operación de copiado de %s al local abortada.\n", copia->nombre_archivo);
				log_info(logger, mensaje);
				free(mensaje);

				free(nombre_con_fin);
				free(copia->nombre_archivo);
				free(copia->motivo);
				free(copia);
			}

			return 0;
		}

//		cantidad_de_veces_maxima = nueva_cantidad_maxima;

//		printf("el mínimo que alguien estuvo fue %d\n", cantidad_de_veces_minima); fflush(stdout);

		list_add(nodos_pedidos, nodo_que_menos_estuvo);


		pthread_cancel(nodo_que_menos_estuvo->hilo);


		char *datos = pedir_bloque(nodo_que_menos_estuvo, bloque_a_pedir);

		pthread_t hilo_mantener_nodo;
		pthread_create(&hilo_mantener_nodo, NULL, (void*) mantener_conexion_nodo, (void*) nodo_que_menos_estuvo->socket);
		nodo_que_menos_estuvo->hilo = hilo_mantener_nodo;


		int hasta = buscar_ultimo_enter(datos, BLOCKSIZE);


		temporal = fopen(nombre_con_fin, "a");
		fwrite(datos, hasta + 1, 1, temporal);

		free(datos);


		fclose(temporal);
	}



	if (string_starts_with(copia->motivo, "copia-"))
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "Archivo %s copiado al local.\n", copia->nombre_archivo);
		log_info(logger, mensaje);
		free(mensaje);

		persistir_archivos();
		persistir_nodos();
	}

	free(nombre_con_fin);
	free(copia->nombre_archivo);
	free(copia->motivo);
	free(copia);

	list_destroy(nodos_pedidos);

	return 1;

}

char *pedir_bloque(nodo* nodo, int num_bloque)
{
	uint32_t operacion = 2; //get bloque
	uint32_t bloque = num_bloque;

	uint32_t size_to_send;
	uint32_t size_total_para_enviar = sizeof(operacion) + sizeof(bloque);

	char* paquete_serializado = malloc(size_total_para_enviar);

	int offset = 0;

	size_to_send = sizeof(operacion);
	memcpy(paquete_serializado+offset, &operacion, size_to_send);
	offset += size_to_send;

	size_to_send = sizeof(bloque);
	memcpy(paquete_serializado+offset, &bloque, size_to_send);
	offset += size_to_send;

	enviar_datos(nodo->socket, paquete_serializado, size_total_para_enviar);

	free(paquete_serializado);


	char *datos = malloc(BLOCKSIZE);

	recv(nodo->socket, datos, BLOCKSIZE, MSG_WAITALL);

	return datos;
}


void borrar_este_bloque(int num_bloque, char* nombre_archivo, nodo* nodo)
{
	nodo->espacio_utilizado -= 20;

	nombre_global_a_buscar = nombre_archivo;
	archivo* archivo = list_find(archivos, (void*) es_este_archivo);

	nombre_global_a_buscar = nodo->nombre;
	t_list *lista_filtrada = list_filter(archivo->lista_bloques_datos, (void *) es_bloque_de_este_nodo);

	bloque_global_a_buscar = num_bloque;
	bloque_datos *bloque_buscado = list_find(lista_filtrada, (void *) es_este_bloque_segun_archivo);

	int i = 0;
	for(i = 0; list_size(archivo->lista_bloques_datos); i++)
	{
		bloque_datos *bloque_actual = list_get(archivo->lista_bloques_datos, i);

		if (bloque_actual == bloque_buscado)
		{
			list_remove(archivo->lista_bloques_datos, i);
			break;
		}
	}

	for(i = 0; list_size(bloques); i++)
	{
		bloque_datos *bloque_actual = list_get(bloques, i);

		if (bloque_actual == bloque_buscado)
		{
			list_remove_and_destroy_element(bloques, i, (void *) destructor_bloque);
			break;
		}
	}
}

int copiar_este_bloque(char *nombre_nodo_a_copiar, char *nombre_archivo, int padre, int num_bloque_archivo)
{
	nodo *nodo_a_copiar = obtener_nodo_por_nombre(nombre_nodo_a_copiar);

	if (nodo_a_copiar->estado == false)
		return -1;

	//nodo_a_copiar->espacio_utilizado += 20;

	archivo *archivo = devolver_este_archivo_con_padre(nombre_archivo, padre);

	int i;
	int bloque_enviado = -1;
	for (i = 0; i < list_size(archivo->lista_bloques_datos); i++)
	{
		bloque_datos *bloque_actual = list_get(archivo->lista_bloques_datos, i);

		if (bloque_actual->num_bloque_archivo == num_bloque_archivo)
		{

			//recibiendo
			nodo *nodo_a_recibir = obtener_nodo_por_nombre(bloque_actual->nodo_padre);

			if (nodo_a_recibir->estado == true)
			{
				pthread_cancel(nodo_a_recibir->hilo);

				char *datos = pedir_bloque(nodo_a_recibir, bloque_actual->bloque_nodo_padre);

				pthread_t hilo_mantener_nodo;
				pthread_create(&hilo_mantener_nodo, NULL, (void*) mantener_conexion_nodo, (void*) nodo_a_recibir->socket);
				nodo_a_recibir->hilo = hilo_mantener_nodo;


				//enviando
				int bloque_libre = buscar_bloque_libre(nodo_a_copiar);
				if (bloque_libre == -1)
					return -1;

				envio_bloque *bloque_a_enviar = malloc(sizeof(envio_bloque));
				bloque_a_enviar->bloque_archivo = num_bloque_archivo;
				bloque_a_enviar->bloque_nodo = bloque_libre;
				bloque_a_enviar->datos = datos;
				bloque_a_enviar->nodo = nodo_a_copiar;
				bloque_a_enviar->num_copia = 0;
				bloque_a_enviar->motivo = 2;

				enviar_bloque(bloque_a_enviar);

				bloque_enviado = 1;

				bloque_datos *bloque_nuevo = malloc(sizeof(bloque_datos));
				bloque_nuevo->bloque_nodo_padre = bloque_libre;
				bloque_nuevo->nodo_padre = malloc(strlen(nodo_a_recibir->nombre) + 1);
				bloque_nuevo->nodo_padre = string_duplicate(nodo_a_copiar->nombre);
				bloque_nuevo->num_bloque_archivo = num_bloque_archivo;

				list_add(archivo->lista_bloques_datos, bloque_nuevo);

				free(datos);

				break;
			}
		}
	}

	return bloque_enviado;

}


void chequear_disponibilidad_archivos()
{
	int i = 0;
	for (i = 0; i < list_size(archivos); i++)
	{
		archivo *archivo_actual = list_get(archivos, i);

		if (list_size(archivo_actual->lista_bloques_datos) != 0)
		{

			int cantidad_de_bloques = cuantos_bloques_tiene(archivo_actual);

	//		printf("estamos con el archivo %s que tiene %d bloques\n", archivo_actual->nombre, cantidad_de_bloques);

			int j = 0;
			for(j = 1; j <= cantidad_de_bloques; j++)
			{
				bloque_global_a_buscar = j;
				t_list *lista_para_este_bloque = list_filter(archivo_actual->lista_bloques_datos, (void *) es_copia_de_este_bloque);

	//			printf("la cantidad de copias que tiene el bloque %d es %d\n", j, list_size(lista_para_este_bloque));

				int k = 0;
				int cantidad_nodos_desconectados = 0;
				for(k = 0; k < list_size(lista_para_este_bloque); k++)
				{
					bloque_datos *bloque_actual = list_get(lista_para_este_bloque, k);

					nodo *nodo_actual = obtener_nodo_por_nombre(bloque_actual->nodo_padre);

					if (nodo_esta_desconectado(nodo_actual))
					{
						cantidad_nodos_desconectados++;
	//					printf("hay %d nodos desconectados\n", cantidad_nodos_desconectados);
					}
					else
					{
						archivo_actual->estado = true;
						break;
					}

				}

	//			printf("hay %d nodos desconectados en %d copias\n", cantidad_nodos_desconectados, list_size(lista_para_este_bloque));
				if (cantidad_nodos_desconectados == list_size(lista_para_este_bloque))
				{	archivo_actual->estado = false;
					break; 							//nuevo
				}

				list_destroy(lista_para_este_bloque);

			}

		}
		else
			list_remove(archivos, i);

	}

}

char* obtener_archivo_de_ruta(char* ruta, int ultimo_enter)
{
	int i = 0;

	int cantidad_de_barras = 0;

	for (i = 0; i < ultimo_enter; i++)
	{
		if (ruta[i] == '/')
			cantidad_de_barras++;
	}

	char **split = string_split(ruta, "/");

	if (cantidad_de_barras == 0)
		return ruta;

	else
	{
		if(ruta[0] != '/')
			return split[cantidad_de_barras];
		else
		{
			char *padre = split[cantidad_de_barras - 1];
			return padre;
		}
	}
}

char* obtener_padre_de_archivo_de_ruta(char* ruta, int ultimo_enter)
{
	int i = 0;
	int cantidad_de_barras = 0;

	for (i=0; i < ultimo_enter; i++)
	{
		if (ruta[i] == '/')
			cantidad_de_barras++;
	}

	if (cantidad_de_barras <= 1) return "/";

	char **split = string_split(ruta, "/");
	char *padre = split[cantidad_de_barras - 2];

	return padre;
}

void mostrar_informacion_archivos()
{
	int i = 0;
	for(i = 0; i < list_size(archivos); i++)
	{
		archivo *archivo_actual = list_get(archivos, i);
		list_sort(archivo_actual->lista_bloques_datos, (void *) ordenar_por_numero_de_bloque_menor);
		printf("\nBloques de %s en %s:\n", archivo_actual->nombre, concatenar_padres(sistema[archivo_actual->padre]/*->directorio*/));

		char* mensaje = malloc(200);
		sprintf(mensaje, "\nBloques de %s en %s:\n", archivo_actual->nombre, concatenar_padres(sistema[archivo_actual->padre]));
		log_info(logger_bloques, mensaje);
		free(mensaje);

		ver_informacion_bloques(archivo_actual->lista_bloques_datos);

	}
}

void ver_informacion_bloques(t_list *lista)
{
	int i = 0;
	for(i = 0; i < list_size(lista); i++)
	{
		bloque_datos *bloque_actual = list_get(lista, i);
		printf("Bloque de datos %d en bloque %d de nodo %s\n",
		bloque_actual->num_bloque_archivo,
		bloque_actual->bloque_nodo_padre,
		bloque_actual->nodo_padre);

		char* mensaje = malloc(200);
		sprintf(mensaje, "Bloque de datos %d en bloque %d en nodo %s",
		bloque_actual->num_bloque_archivo,
		bloque_actual->bloque_nodo_padre,
		bloque_actual->nodo_padre);
		log_info(logger_bloques, mensaje);
		free(mensaje);

	}
}
void *agregar_archivo(char* ruta, int directorio_padre, char *nombre)
{
	FILE *archivo_a_agregar;
	archivo_a_agregar = fopen(ruta, "r+");

	if (archivo_a_agregar == NULL)
	{
		printf("El archivo no existe o no se puede abrir.\n");
		return 0;
	}

	t_list *bloques_datos = partir(archivo_a_agregar);

	if (list_size(bloques_datos) == 0)
	{
		char* mensaje = malloc(100);
		sprintf(mensaje, "Operación de agregado de %s abortada.\n", nombre);
		log_info(logger, mensaje);
		free(mensaje);
		return 0;
	}

	t_list *bloques_para_archivo = list_create();
	list_add_all(bloques_para_archivo, bloques_copiandose);
	list_clean(bloques_copiandose);

	archivo *nuevo_archivo = malloc(sizeof(archivo));
	assert(nuevo_archivo != NULL);

	nuevo_archivo->nombre = strdup(nombre);
	nuevo_archivo->padre = directorio_padre;
	nuevo_archivo->tamanio = filesize(archivo_a_agregar);
	nuevo_archivo->estado = true;
	nuevo_archivo->lista_bloques_datos = bloques_para_archivo;

//	printf("estoy creando un archivo: %s, le pongo estos bloques: %d\n", nombre, list_size(bloques_para_archivo));

	fclose(archivo_a_agregar);

	list_add(archivos, nuevo_archivo);

	char* mensaje = malloc(100);
	sprintf(mensaje, "%s agregado exitosamente.\n", nombre);
	log_info(logger, mensaje);
	free(mensaje);

	return 0;



//	return nuevo_archivo;
}

void destructor_archivo(archivo* archivo)
{
	free(archivo->nombre);
	list_destroy_and_destroy_elements(archivo->lista_bloques_datos, (void *) destructor_bloque);
	free(archivo);
}

void eliminar_archivo(char *nombre, int directorio_padre)
{
	nombre_global_a_buscar = nombre;

	archivo *archivo = list_find(archivos, (void*) es_este_archivo);

	list_remove_by_condition(archivos, (void*) es_este_archivo);

	int i = 0;
	for (i = 0; i < list_size(archivo->lista_bloques_datos); i++)
	{
		bloque_datos *bloque_actual = list_get(archivo->lista_bloques_datos, i);

		nombre_global_a_buscar = bloque_actual->nodo_padre;
		bloque_global_a_buscar = bloque_actual->bloque_nodo_padre;

		nodo *nodo_padre = list_find(nodos, (void*) buscar_nodo_por_nombre);

		nodo_padre->espacio_utilizado -= 20;

		list_remove_by_condition(bloques, (void*) es_este_bloque_segun_nodo);
	}

	destructor_archivo(archivo);

}

bool es_este_archivo(archivo *archivo)
{
	bool coincide_nombre	= string_starts_with(archivo->nombre, nombre_global_a_buscar);
	bool coincide_padre 	= archivo->padre == padre_global_a_buscar;

	return (coincide_nombre && coincide_padre);
}

void* buscar_archivo_por_nombre(archivo *archivo)
{
	if (string_starts_with(archivo->nombre, nombre_global_a_buscar))
	{
		return archivo;
	}

	return 0;
}

void mover_archivo(char *nombre_archivo, int padre_actual, int nuevo_padre)
{
	archivo *archivo_a_mover = devolver_este_archivo_con_padre(nombre_archivo, padre_actual);
	archivo_a_mover->padre = nuevo_padre;
}

void renombrar_archivo(char* archivo_a_renombrar, int directorio_padre, char *nuevo_nombre)
{
	padre_global_a_buscar = directorio_padre;
	nombre_global_a_buscar = archivo_a_renombrar;

	archivo *archivo = list_find(archivos, (void*) es_este_archivo);

	free(archivo->nombre);

	archivo->nombre = strdup(nuevo_nombre);
}

void obtener_md5(lectura_archivo *md5)
{
	char *nombre_con_md5 = string_duplicate("MD5-");
	string_append(&nombre_con_md5, md5->nombre_archivo);

	int exito = copiar_archivo_al_local(md5);

	if (exito == 0)
	{
		printf("No se puede obtener el MD5 de %s.\n", md5->nombre_archivo);
		free(md5->nombre_archivo);
		free(md5);
		return;
	}

	char string[100];
	memset(string, '\0', sizeof(100));
	sprintf(string, "md5sum %s", nombre_con_md5);
	system(string);

	remove(nombre_con_md5);
	free(nombre_con_md5);
//	free(md5->nombre_archivo);
//	free(md5->motivo);
//	free(md5);

	return;
}


bool es_archivo_hijo_de_padre_global(archivo* archivo)
{
	return (archivo->padre == padre_global_a_buscar);
}

int archivo_tiene_nombre_legal(char *nombre)
{
	int i = 0;
	for (i = 0; i < strlen(nombre); i++)
	{
		if (nombre[i] == '/')
			return 0;
	}

	int existe = existe_archivo(nombre);

	if (existe == 1) return 0;

	return 1;
}

int existe_archivo(char *nombre)
{
	int i = 0;
	for(i = 0; i < list_size(archivos); i++)
	{
		nombre_global_a_buscar = nombre;

		if(	list_any_satisfy(archivos, (void *) es_este_archivo)	)
		{
			return 1;
		}

	}

	return 0;
}

un_directorio *crear_directorio(char *directorio, int padre) //devuelve el puntero al directorio creado
{
	un_directorio *nuevo_directorio = malloc(sizeof(un_directorio));	//dame memoria, crearé un directorio
	assert(nuevo_directorio != NULL);									//si es que se puede

	nuevo_directorio->directorio = strdup(directorio);
	nuevo_directorio->padre = padre;					//guardando sus datos...

	int i = 1;
	while(ocupado[i] != 0)	//se fija en qué parte de las 1023 se puede.
	{						//no debe haber nada guardado allí!
		i++;
							//si llega a irse a 1024, explota.
	}

	nuevo_directorio->posicion = i;		//además, el directorio sabe dónde está guardado.

	sistema[i] = nuevo_directorio;		//guardo el puntero a este directorio.
	ocupado[i] = 1;						//ahora esta posición está ocupada. nadie más guardará ahí.

	persistir_directorios();

	return nuevo_directorio;
};


void eliminar_directorio(un_directorio *directorio)
{ //TODO
	assert(directorio != NULL);
	int posicion_sistema = directorio->posicion;

	padre_global_a_buscar = directorio->posicion;

	if(posicion_sistema != 0)
	{

		int i = 1;
		while(i<=1023)
		{
			if ( (ocupado[i] == 1) && (i != posicion_sistema) )
			{
				if( (sistema[i]->padre == posicion_sistema) && (sistema[i]->padre != 0) )
				{
					eliminar_directorio(sistema[i]);
				}
			}
			i++;
		}

		ocupado[posicion_sistema] = 0;

		free(directorio->directorio);
		free(directorio);

	}



	int i = 0;

	for(i = 0; i < list_size(archivos); i++)
	{
		if (list_find(archivos, (void*) es_archivo_hijo_de_padre_global) == 0) break;

		archivo *archivo_actual = list_find(archivos, (void*) es_archivo_hijo_de_padre_global);
		eliminar_archivo(archivo_actual->nombre, archivo_actual->padre);

	}

	persistir_directorios();

}

void mover_directorio(un_directorio *directorio, int nuevo_padre)
{
	//if(directorio->posicion != 0)
	//{
		directorio->padre = nuevo_padre;
	//}

	persistir_directorios();
}

void renombrar_directorio(un_directorio *directorio, char *nuevo_nombre)
{
	//if(directorio->posicion != 0)
	//{
		free(directorio->directorio);
		directorio->directorio = strdup(nuevo_nombre);
	//}

	persistir_directorios();
}

void formatear()
{
	int i = 1;
	for (i = 1; i < 1024; i++)
	{
		if(ocupado[i]==1)
			eliminar_directorio(sistema[i]);
	}

	list_clean_and_destroy_elements(archivos, (void *) destructor_archivo);
	//list_clean_and_destroy_elements(bloques, (void *) destructor_bloque);
	list_clean(bloques);

	i = 0;
	for(i = 0; i < list_size(nodos); i++)
	{
		nodo* nodo_actual = list_get(nodos, i);
		nodo_actual->espacio_utilizado = 0;
	}

	persistir_nodos();
	persistir_archivos();

}

void mostrar_directorios_hijos(un_directorio *directorio)
{
	if (directorio == NULL)
		printf("El directorio a mostrar no existe.\n");
	else
	{
		int i = 0;
		for(i = 0; i <= 1024; i++);
		{
			if(ocupado[i]==1 && sistema[i]->padre == directorio->posicion)
				printf("%s\n", directorio->directorio);
		}
	}
}

char* obtener_directorio_padre(un_directorio *directorio)
{
	int padre = directorio->padre;
	char* direccion = sistema[padre]->directorio;

	return direccion;
}

int cuantos_directorios_ocupados()
{
	int cuantos = 0;
	int p;
	for (p = 0; p < 1024; p++)
	{
		if(ocupado[p]==1)
			cuantos++;
	}

	return cuantos;
}

void mostrar_sistema() //muestra todos los directorios
{
	int i = 0;
	while (i <= 1023)
	{
		if (ocupado[i] == 1)
		{
			printf("En %d y con", sistema[i]->posicion);
			printf("padre %d,", sistema[i]->padre);
			printf("el directorio %s\n", sistema[i]->directorio);
		}

		i++;
	}
}

void mostrar_hijos_de(int padre)
{
	int i = 1;
	while (i <= 1023)
	{
		if (ocupado[i] == 1 && sistema[i]->padre == padre)
		{
			printf("%s\n", sistema[i]->directorio);
		}

		i++;
	}

	padre_global_a_buscar = padre;
	t_list *archivos_hijos = list_filter(archivos, (void*) es_archivo_hijo_de_padre_global);

	for(i = 0; i < list_size(archivos_hijos); i++)
	{
		archivo *archivo_actual = list_get(archivos_hijos, i);
		printf("%s", archivo_actual->nombre);

		if (archivo_actual->estado == false)
			printf(" (no disponible)");

		printf("\n");

	}

	list_destroy(archivos_hijos);

}

int buscar_hijo_de(int padre, char *hijo)
{
	int i = 0;
	while (i <= 1023)
	{
		if (ocupado[i] == 1 && sistema[i]->padre == padre && string_starts_with(sistema[i]->directorio, hijo))
		{
			return i;
		}

		i++;
	}

	return 1025;
}

int es_subdirectorio(un_directorio *directorio, int padre_lider)
{
	if (directorio->padre == 0)
		return 0;
	if (directorio->padre == padre_lider)
		return 1;
	if (directorio->padre != padre_lider)
		es_subdirectorio(sistema[directorio->padre], padre_lider);

	return 0;
}

int directorio_tiene_nombre_legal(char *nombre)
{

	int i = 0;
	for (i = 0; i < strlen(nombre); i++)
	{
		if (nombre[i] == '/')
		{
			return 0;
		}
	}

	for(i = 1; i <= 1023; i++)
	{
		if(ocupado[i] == 1 && (string_starts_with(nombre, sistema[i]->directorio)))
		{
			return 0;
		}
	}

	return 1;
}

int cuantos_bloques_tiene(archivo* archivo)
{
	t_list *lista = archivo->lista_bloques_datos;
	list_sort(lista, (void *) ordenar_por_numero_de_bloque_mayor);

	bloque_datos *bloque_mayor = list_get(lista, 0);

	return bloque_mayor->num_bloque_archivo;
}

bool ordenar_por_numero_de_bloque_mayor(bloque_datos* bloque_uno, bloque_datos* bloque_dos)
{
	return bloque_uno->num_bloque_archivo > bloque_dos->num_bloque_archivo;
}

bool ordenar_por_numero_de_bloque_menor(bloque_datos* bloque_uno, bloque_datos* bloque_dos)
{
	return bloque_uno->num_bloque_archivo < bloque_dos->num_bloque_archivo;
}
