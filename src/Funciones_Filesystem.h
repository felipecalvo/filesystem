#ifndef FUNCIONES_FILESYSTEM_H_
#define FUNCIONES_FILESYSTEM_H_

//ver cuáles se usan
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <pthread.h>
#include <animal.h>
//#include <sys/mman.h> ?

#include "animal.h"

//#define IPMARTA "127.0.0.1" borrable
//#define PUERTOMARTA "2000" borrable
#define BLOCKSIZE 1024*1024*20


typedef struct _un_directorio
{
	char *directorio;
	int padre;
	int posicion;

} un_directorio;

typedef struct _archivo
{
	char *nombre;
	int padre;
	float tamanio;
	bool estado;
	t_list *lista_bloques_datos; //apunta a bloque_datos

} archivo;

typedef struct _nodo
{
	char *nombre;
	char *ip;
	char *puerto;
	int socket;
	int espacio_total;
	int espacio_utilizado;
	bool estado;
	pthread_t hilo;

} nodo;

typedef struct _bloque_datos
{
	int num_bloque_archivo;
	char *nodo_padre;
	int bloque_nodo_padre;

} bloque_datos;

typedef struct _envio_bloque
{
	int num_copia;
	int bloque_archivo;
	int bloque_nodo;
	char* datos;
	nodo *nodo;
	int motivo;	// 0 para un bloque cualquiera, 1 si es para marta, 2 si es un individual

} envio_bloque;

typedef struct _lectura_archivo
{
	char *nombre_archivo;
	int padre;
	char* motivo;

} lectura_archivo;

void inicializar();
void cargar_directorios();
void cargar_nodos();
void cargar_archivos();
void persistir_directorios();
void persistir_nodos();
void persistir_archivos();

int conectar_marta(int socket_servidor);
void mantener_conexion_marta(int socket_cliente_marta);
void marta_obtener_bloques(int socket_cliente_marta);
uint32_t size_lista_nodos();
void marta_solicitar_nodos();
void marta_solicitar_resultado();
void marta_solicitar_nodos_caidos();

int filesize(FILE *archivo);
char *mmapear_blocksize(int fd_archivo, int cuanto, int desde);
int buscar_ultimo_enter(char *mmap, int cuanto);
void llenar_barraceros(int ultimo_enter, char *mmap, int cuanto);
char* guardar_offset(int tamanio_offset, char *mmap, int cuanto);
t_list *partir(FILE *archivo);

void conectar_nodos(int socket_servidor);
int triplicar_copia_nodos(char* datos, int numero_de_bloque_archivo, int motivo);
void enviar_bloque(envio_bloque *copia);
int buscar_bloque_libre(nodo* nodo);
void eliminar_bloques_fallidos(t_list *bloques_fallidos);
bool es_bloque_de_este_nodo(bloque_datos *bloque);
bool esta_ocupado_el_bloque_global(bloque_datos *bloque);
bool es_este_bloque_segun_nodo(bloque_datos *bloque);
void destructor_bloque(bloque_datos *bloque);
t_list *tres_nodos_convenientes();
bool ordenar_por_espacio_utilizado(nodo* nodo_ocupado, nodo* nodo_mas_ocupado);
bool ordenar_por_tamanio(nodo* nodo_ocupado, nodo* nodo_mas_ocupado);
nodo* obtener_nodo_por_nombre(char *nombre);
bool nodo_esta_lleno(nodo* nodo);
bool nodo_esta_desconectado(nodo* nodo);
int agregar_nodo(int socket_servidor);
void mantener_conexion_nodo(int socket_nodo_conectado);
int buscar_nodo_por_socket(nodo *nodo);
int buscar_nodo_por_nombre(nodo *nodo);
bool coincide_nodo_con_nombre(nodo *nodo);
void mostrar_nodos();
void mostrar_nodo(nodo* nodo);
nodo* guardar_informacion_nodo(int socket_nodo);
bool esta_conectado(nodo *nodo);
char* eliminar_nodo(char *nombre_nodo);
void destructor_nodo(nodo *nodo);
void desconectar_nodo();
bool es_nodo_caido(char* nombre_nodo);
uint32_t tamanio_lista_nodos(t_list* lista_nodos);

int tiene_este_bloque(char *nombre_nodo, char *nombre_archivo, int padre, int num_bloque);
int copiar_archivo_al_local(lectura_archivo *copia);
char *pedir_bloque(nodo* nodo, int num_bloque);
void chequear_disponibilidad_archivos();
char* obtener_archivo_de_ruta(char* ruta, int ultimo_enter);
char* obtener_padre_de_archivo_de_ruta(char* ruta, int ultimo_enter);
void mostrar_informacion_archivos();
void ver_informacion_bloques(t_list *lista);
void *agregar_archivo(char* ruta, int directorio_padre, char *nombre);
void mover_archivo(char *nombre_archivo, int padre_actual, int nuevo_padre);
void destructor_archivo(archivo* archivo);
void eliminar_archivo(char *nombre, int directorio_padre);
bool es_este_archivo(archivo *archivo);
void* buscar_archivo_por_nombre(archivo *archivo);
void renombrar_archivo(char* archivo_a_renombrar, int directorio_padre, char *nuevo_nombre);
void obtener_md5(lectura_archivo *md5);
bool es_archivo_hijo_de_padre_global(archivo* archivo);
int archivo_tiene_nombre_legal(char *nombre);
int existe_archivo(char *nombre);
void *devolver_este_archivo(char *nombre);
void *devolver_este_archivo_con_padre(char *nombre, int padre);
bool es_copia_de_este_bloque(bloque_datos *bloque);
void borrar_este_bloque(int num_bloque, char* nombre_archivo, nodo* nodo);
int copiar_este_bloque(char *nombre_nodo_a_copiar, char *nombre_archivo, int padre, int num_bloque_archivo);
void destructor_nombre(char* nombre);

un_directorio *crear_directorio(char *directorio, int padre);
void eliminar_directorio(un_directorio *directorio);
void mover_directorio(un_directorio *directorio, int nuevo_padre);
void renombrar_directorio(un_directorio *directorio, char *nuevo_nombre);

void mostrar_directorios_hijos(un_directorio *directorio);
char* obtener_directorio_padre(un_directorio *directorio);
char* concatenar_padres(un_directorio *directorio);

int cuantos_directorios_ocupados();
void mostrar_sistema();
void mostrar_hijos_de(int padre);
int buscar_hijo_de(int padre, char *hijo);
int es_subdirectorio(un_directorio *directorio, int padre_lider);
int directorio_tiene_nombre_legal(char *nombre);
int cuantos_bloques_tiene(archivo* archivo);
bool ordenar_por_numero_de_bloque_mayor(bloque_datos* bloque_uno, bloque_datos* bloque_dos);
bool ordenar_por_numero_de_bloque_menor(bloque_datos* bloque_uno, bloque_datos* bloque_dos);

int consola();
void formatear();


void tests();

#endif /* FUNCIONES_FILESYSTEM_H_ */
