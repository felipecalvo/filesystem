all:
	gcc -I"../Library_animal" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Funciones_Filesystem.d" -MT"Funciones_Filesystem.d" -o "Funciones_Filesystem.o" "src/Funciones_Filesystem.c"
 
	gcc -I"../Library_animal" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Filesystem.d" -MT"Filesystem.d" -o "Filesystem.o" "src/Filesystem.c"

	gcc -L"../Library_animal" -o "Filesystem"  ./Funciones_Filesystem.o ./Filesystem.o   -lLibrary_animal -lcommons -lpthread
