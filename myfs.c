/*
*  myfs.c - Implementacao do sistema de arquivos MyFS
*
*  Autores: Quezia Emanuelly da Silva Oliveira
*  Projeto: Trabalho Pratico II - Sistemas Operacionais
*  Organizacao: Universidade Federal de Juiz de Fora
*  Departamento: Dep. Ciencia da Computacao
*
*/

#include <stdio.h>
#include <stdlib.h>
#include "myfs.h"
#include "vfs.h"
#include "inode.h"
#include "util.h"

#define INDEX_TOTALBLOCKS 0
#define INDEX_BLOCKSIZE 4
#define INDEX_BITMAP 8
#define ID_INODE_DEFAULT 1
#define QUANT_NUM_INODES 20
// #define BLOCK_INODES 1

FSInfo* fileSystem;
int* emptyBlocks;

unsigned char* bitMap;
unsigned int systemBlockSize;
unsigned int totalBlocks;


//Declaracoes globais

//Funcao para verificacao se o sistema de arquivos está ocioso, ou seja,
//se nao ha quisquer descritores de arquivos em uso atualmente. Retorna
//um positivo se ocioso ou, caso contrario, 0.
int myFSIsIdle (Disk *d) {
	return 0;
}

//Funcao para formatacao de um disco com o novo sistema de arquivos
//com tamanho de blocos igual a blockSize. Retorna o numero total de
//blocos disponiveis no disco, se formatado com sucesso. Caso contrario,
//retorna -1.
int myFSFormat (Disk *d, unsigned int blockSize) {
	if(d != NULL) {
		unsigned char superBlock[DISK_SECTORDATASIZE] = {0};
		unsigned char aux[DISK_SECTORDATASIZE] = {0};
		unsigned char clearSectores[DISK_SECTORDATASIZE] = {0};

		for(int i = 0; i < diskGetNumSectors(d); i++) {
			if(diskWriteSector(d,i,clearSectores) == -1) return -1;
		}

		totalBlocks = diskGetSize(d) / blockSize;		
		
		ul2char(totalBlocks, &superBlock[INDEX_TOTALBLOCKS]);
		printf("\ntotal block hex: 0x%02X\n", superBlock[INDEX_TOTALBLOCKS]);
		printf("total block decimal: %d\n", totalBlocks);
					
		
		ul2char(blockSize, &superBlock[INDEX_BLOCKSIZE]);
		printf("block size hex: 0x%02X\n", superBlock[INDEX_BLOCKSIZE]);
		printf("block size decimal: %d\n", blockSize);
		
		int tamBitMap = (totalBlocks/8);
		bitMap = malloc(tamBitMap);
		memset(bitMap,0,tamBitMap);
		bitMap[0] = 1; //coloca o bloco do super bloco como ocupado
		memcpy(&superBlock[INDEX_BITMAP], bitMap, tamBitMap); //copia os dados do bitmap pro superblock
		
		if(diskWriteSector(d,0,superBlock) == -1) return -1;
		
		if(diskReadSector(d,0,aux) == -1) return -1;
		for(int i = 0; i <= INDEX_BITMAP; i++) {
			printf("indice: %d: 0x%02X\n", i, aux[i]);
		}
		

		// if(diskWriteSector(d,0,superBlock) == -1) return -1;

		// char* dataSystem = malloc(512);
		// memset(dataSystem,0,512);
		// snprintf(dataSystem, 512, "id:%c;name:%s;totalBlocks:%d", fileSystem->fsid, fileSystem->fsname, totalBlocks);
		// if(diskWriteSector(d,0,dataSystem) == -1) return -1; //escreve no setor zero as informações do file system
		// free(dataSystem);

		// char* value = malloc(512);
		// memset(value,0,512);
		// snprintf(value, 512, "0");
		// for(int i = 1; i < diskGetNumSectors(d); i++) {
		// 	if(diskWriteSector(d,i,value) == -1) return -1; //escreve zero em todos os setores do disco
		// }
		// free(value);

		// for(int i = 1; i <= QUANT_NUM_INODES; i++) {
		// 	if(inodeCreate(i, d) == NULL) return -1; //cria os 20 inodes e armazena no disco
		// }
		return 0;
	}
	return -1;
}

//Funcao para abertura de um arquivo, a partir do caminho especificado
//em path, no disco montado especificado em d, no modo Read/Write,
//criando o arquivo se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpen (Disk *d, const char *path) {
	return -1;
}
	
//Funcao para a leitura de um arquivo, a partir de um descritor de
//arquivo existente. Os dados lidos sao copiados para buf e terao
//tamanho maximo de nbytes. Retorna o numero de bytes efetivamente
//lidos em caso de sucesso ou -1, caso contrario.
int myFSRead (int fd, char *buf, unsigned int nbytes) {
	return -1;
}

//Funcao para a escrita de um arquivo, a partir de um descritor de
//arquivo existente. Os dados de buf serao copiados para o disco e
//terao tamanho maximo de nbytes. Retorna o numero de bytes
//efetivamente escritos em caso de sucesso ou -1, caso contrario
int myFSWrite (int fd, const char *buf, unsigned int nbytes) {
	return -1;
}

//Funcao para fechar um arquivo, a partir de um descritor de arquivo
//existente. Retorna 0 caso bem sucedido, ou -1 caso contrario
int myFSClose (int fd) {
	return -1;
}

//Funcao para abertura de um diretorio, a partir do caminho
//especificado em path, no disco indicado por d, no modo Read/Write,
//criando o diretorio se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpenDir (Disk *d, const char *path) {
	return -1;
}

//Funcao para a leitura de um diretorio, identificado por um descritor
//de arquivo existente. Os dados lidos correspondem a uma entrada de
//diretorio na posicao atual do cursor no diretorio. O nome da entrada
//e' copiado para filename, como uma string terminada em \0 (max 255+1).
//O numero do inode correspondente 'a entrada e' copiado para inumber.
//Retorna 1 se uma entrada foi lida, 0 se fim de diretorio ou -1 caso
//mal sucedido
int myFSReadDir (int fd, char *filename, unsigned int *inumber) {
	return -1;
}

//Funcao para adicionar uma entrada a um diretorio, identificado por um
//descritor de arquivo existente. A nova entrada tera' o nome indicado
//por filename e apontara' para o numero de i-node indicado por inumber.
//Retorna 0 caso bem sucedido, ou -1 caso contrario.
int myFSLink (int fd, const char *filename, unsigned int inumber) {
	return -1;
}

//Funcao para remover uma entrada existente em um diretorio, 
//identificado por um descritor de arquivo existente. A entrada e'
//identificada pelo nome indicado em filename. Retorna 0 caso bem
//sucedido, ou -1 caso contrario.
int myFSUnlink (int fd, const char *filename) {
	return -1;
}

//Funcao para fechar um diretorio, identificado por um descritor de
//arquivo existente. Retorna 0 caso bem sucedido, ou -1 caso contrario.	
int myFSCloseDir (int fd) {
	return -1;
}

//Funcao para instalar seu sistema de arquivos no S.O., registrando-o junto
//ao virtual FS (vfs). Retorna um identificador unico (slot), caso
//o sistema de arquivos tenha sido registrado com sucesso.
//Caso contrario, retorna -1
int installMyFS (void) {
	fileSystem = malloc(sizeof(FSInfo));
	char* name = "teste";
	
	fileSystem->fsid = 'S';
	fileSystem->fsname = name;
	fileSystem->isidleFn = myFSIsIdle;
	fileSystem->formatFn = myFSFormat;
	fileSystem->openFn = myFSOpen;
	fileSystem->readFn = myFSRead;
	fileSystem->writeFn = myFSWrite;
	fileSystem->closeFn = myFSClose;
	fileSystem->opendirFn = myFSOpenDir;
	fileSystem->readdirFn = myFSReadDir;
	fileSystem->linkFn = myFSLink;
	fileSystem->unlinkFn = myFSUnlink;
	fileSystem->closedirFn = myFSCloseDir;
	
	if(fileSystem->fsname == NULL || fileSystem->isidleFn == NULL || 
		fileSystem->formatFn == NULL || fileSystem->openFn == NULL || fileSystem->readFn == NULL || 
		fileSystem->writeFn == NULL || fileSystem->closeFn == NULL || fileSystem->opendirFn == NULL || 
		fileSystem->readdirFn == NULL || fileSystem->linkFn == NULL || fileSystem->unlinkFn == NULL ||
		fileSystem->closedirFn == NULL) {

			return -1;
	}

	int slot = vfsRegisterFS(fileSystem);

	if(slot == -1) {
		return -1;
	}

	return slot;
}
