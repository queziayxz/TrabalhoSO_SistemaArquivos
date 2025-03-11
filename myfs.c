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
#include <math.h>
#include <string.h>
#include "myfs.h"
#include "vfs.h"
#include "inode.h"
#include "util.h"

#define INDEX_TOTALBLOCKS 0 //index no superbloco para encontrar o total de blocos
#define INDEX_BLOCKSIZE 4 //index no superbloco para encontrar o tamanho do bloco
#define INDEX_SECTOR_INIT 8 //index no superbloco para encontrar o setor inicial livre
#define INDEX_TOTAL_BLOCK_FILE 12 //index no superbloco para encontrar a quantidade de blocos de arquivos
#define INDEX_BLOCK_ROOT 16 //index no superbloco para encontrar o block do diretorio root
#define INDEX_BITMAP 20 //index no superbloco para encontrar o bitmap dos blocos livres

#define ID_INODE_DEFAULT 1 
#define NUM_SECTOR_INIT_INODE 2 //bloco default para começar a armazenar os inodes

#define MAX_INODES 1024 // numero maximo de inodes
#define MAX_FILES 1024
#define MAX_OPEN_FILES 128
#define MAX_FILE_LENGTH 255

#define DIR_RAIZ "/root"

//**************************************************
// VARIÁVEIS GLOBAIS - CRIADAS PELOS ALUNOS
//**************************************************


FSInfo* fileSystem;

typedef struct files {
	Inode* inode;
	const char* name;
	unsigned char* descriptor;
	unsigned int isOpen; // 0 para aberto, 1 para fechado
} FileDescriptor;

typedef struct directoryFileEntry {
	char name[MAX_FILE_LENGTH];
	Inode* inode;
}DirectoryFileEntry;

typedef struct diretory {
	DirectoryFileEntry* files[MAX_FILES];
	unsigned int block;
	int contRef;
} Directory;

Inode* inodes[MAX_INODES];
unsigned char* bitMap;
unsigned int systemBlockSize;
unsigned int totalBlocks;
unsigned int sectorInit;
unsigned int* blockRoot;

Directory diretoryRoot;
FileDescriptor fileDescriptor[MAX_OPEN_FILES];

//**************************************************
// FUNÇÕES PRIVADAS - CRIADAS PELOS ALUNOS
//**************************************************

//função que retorna o numero de um bloco livre no disco
//retorna o numero de bloco caso encontre
//se não encontrar nenhum, retorna -1
int _bitMapGetBlockFree(Disk* d)
{
	unsigned char superBlock[DISK_SECTORDATASIZE] = {0};
	unsigned int* blockFree = malloc(sizeof(unsigned int*));
	unsigned int tamBitMap = 0;
	blockRoot = malloc(sizeof(unsigned int*));

	//ler o super bloco do disco
	diskReadSector(d,0,superBlock);
	printf("\npassou da leitura do setor\n");
	
	//transforma o tamanho do bitmap em int
	char2ul(&superBlock[INDEX_TOTAL_BLOCK_FILE],&tamBitMap);
	printf("passou da transformaxao do tam\n");
	
	//copia o bitmap do superbloco pra variavel global
	bitMap = malloc(tamBitMap);
	memcpy(bitMap, &superBlock[INDEX_BITMAP], tamBitMap);
	printf("copiou os dados do bitmap\n");

	//procura algum bloco livre, se sim retorna a soma do bloco root mais o i do bitmap
	for(int i = 0; i < tamBitMap; i++) {
		char2ul(&superBlock[INDEX_BITMAP+i], blockFree);
		if(!*blockFree){
			char2ul(&superBlock[INDEX_BLOCK_ROOT], blockRoot);
			return *blockRoot+i;
		} 
	}
	return -1;
}

//função que coloca o bloco dado como ocupado
void _bitMapSetFreePerBusy(int blockFree)
{
	bitMap[blockFree - *blockRoot] = 1; 
}

//função que coloca o bloco dado como desocupado
void _bitMapSetBusyPerFree(int blockFree)
{
	bitMap[blockFree - *blockRoot] = 0; 
}

//função que cria todos os inodes suportados no filesystema
void _initInode(Disk *d)
{
	for(int i = 1; i <= MAX_INODES; i++) {
		inodes[i] = inodeCreate(i,d);
		// if(inodes[i] == NULL) {
		// 	printf("\n retornou -1 no init inode");
		// 	return -1;	
		// } 
	}
}

//inicializa o diretório raiz
void _initDirRoot(Disk* d)
{
	
}

//função que cria o diretório raiz
//cria um bloco de dados correspondente ao 
//primeiro bloco livre do disco e salva o inode
//retorna -1 caso não consiga criar o bloco de dados
int _createDirRoot()
{
	diretoryRoot.contRef = 0;//(apagar talvez)

	//pega o id do inode default e seta ele como arquivo de diretório
	inodeSetFileType(inodes[ID_INODE_DEFAULT], 1); // 0 para arquivo regular, 1 para diretório
	inodeSetRefCount(inodes[ID_INODE_DEFAULT], diretoryRoot.contRef);//(apagar talvez)
	
	//cria um novo bloco na lista de blocos do inode
	int idBlock = inodeAddBlock(inodes[ID_INODE_DEFAULT],ceil(sectorInit/8));
	if(idBlock == -1) return -1;

	// pega o bloco correspondente ao id do bloco criado
	diretoryRoot.block =  inodeGetBlockAddr(inodes[ID_INODE_DEFAULT],idBlock); 
	if(inodeSave(inodes[ID_INODE_DEFAULT]) == -1) return -1;

	return 0;

}

//função que cria uma nova entrada 
//no diretório raiz. retorna -1 caso de mal sucedico
//e 1 caso feito com sucesso
int _addDiretoryEntry(Disk* d, const char* filename, Inode* inode)
{
	unsigned char aux[DISK_SECTORDATASIZE] = {0};
	unsigned char superblock[DISK_SECTORDATASIZE] = {0};
	Inode* inodeRoot = inodeLoad(ID_INODE_DEFAULT, d);

	// diskReadSector(d,0,superBlock);
	// char2ul(&superBlock[INDEX_BLOCK_ROOT], &diretoryRoot.block);
	
	if(inodeGetRefCount(inodeRoot) == 0) {
		diretoryRoot.files[inodeGetRefCount(inodeRoot)] = malloc(sizeof(DirectoryFileEntry));
	}

	//verifica se o filename ja existe
	for(int i = 0; i < inodeGetRefCount(inodeRoot); i++) {
		if(strcmp(diretoryRoot.files[i]->name, filename)) return -1; 
	}
	
	strcpy(diretoryRoot.files[inodeGetRefCount(inodeRoot)]->name, filename);
	diretoryRoot.files[inodeGetRefCount(inodeRoot)]->inode = inode;

	return 1;

}

//**************************************************
// FUNÇÕES PUBLICAS
//**************************************************


//Funcao para verificacao se o sistema de arquivos está ocioso, ou seja,
//se nao ha quisquer descritores de arquivos em uso atualmente. Retorna
//um positivo se ocioso ou, caso contrario, 0.
int myFSIsIdle (Disk *d) {
	int contDescriptor = 0;
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(fileDescriptor[i].isOpen) {
			contDescriptor++;
		}
	}

	if(contDescriptor != MAX_OPEN_FILES) return 1;

	return 0;
}

//Funcao para formatacao de um disco com o novo sistema de arquivos
//com tamanho de blocos igual a blockSize. Retorna o numero total de
//blocos disponiveis no disco, se formatado com sucesso. Caso contrario,
//retorna -1.
int myFSFormat (Disk *d, unsigned int blockSize) {
	if(d != NULL) {
		unsigned char superBlock[DISK_SECTORDATASIZE] = {0};
		// unsigned char aux[DISK_SECTORDATASIZE] = {0};
		unsigned char clearSectores[DISK_SECTORDATASIZE] = {0};
		
		//limpa os setores
		for(int i = 0; i < diskGetNumSectors(d); i++) {
			if(diskWriteSector(d,i,clearSectores) == -1) return -1;
		}

		//armazena o valor total de blocos no super bloco
		totalBlocks = diskGetSize(d) / blockSize;		
		ul2char(totalBlocks, &superBlock[INDEX_TOTALBLOCKS]);
		
		//armazena o valor do blocksize no super bloco 
		ul2char(blockSize, &superBlock[INDEX_BLOCKSIZE]);

		//calcula quantos setores será necessário para armazenar todos os inodes e qual o primeiro setor livre
		unsigned int sizeInodeSpace = MAX_INODES/inodeNumInodesPerSector();
		sectorInit = diskGetNumSectors(d) - ((sizeInodeSpace+NUM_SECTOR_INIT_INODE));
		ul2char(sectorInit, &superBlock[INDEX_SECTOR_INIT]);
		
		//cria todos os inodes e armazena no disco
		_initInode(d);
		
		//crio o diretorio raiz e um bloco de dados e armazena no superbloco o bloco do diretorio raiz
		if(_createDirRoot() == -1) return -1;
		ul2char(diretoryRoot.block, &superBlock[INDEX_BLOCK_ROOT]);
		
		//armazena o bitmap dos blocos livres no superbloco
		int tamBitMap = (totalBlocks - ((sizeInodeSpace)/(blockSize/DISK_SECTORDATASIZE))); //calcula quantos bits serao necessarios para armazenar os blocos livres
		ul2char(tamBitMap, &superBlock[INDEX_TOTAL_BLOCK_FILE]);

		bitMap = malloc(tamBitMap);
		memset(bitMap,0,tamBitMap);
		bitMap[0] = 1; // marca como ocupado o primeiro bloco correspondente ao diretório raiz 
		memcpy(&superBlock[INDEX_BITMAP], bitMap, tamBitMap); //copia os dados do bitmap pro superblock
		
		//escreve no bloco zero o superbloco
		if(diskWriteSector(d,0,superBlock) == -1) return -1;

		// if(diskReadSector(d,0,aux) == -1) return -1;
		// for(int i = 0; i <= INDEX_SECTOR_INIT+4; i++) {
		// 	printf("indice: %d: 0x%02X\n", i, aux[i]);
		// }
		return totalBlocks > 0 ? totalBlocks : -1;
	}
	return -1;
}

//Funcao para abertura de um arquivo, a partir do caminho especificado
//em path, no disco montado especificado em d, no modo Read/Write,
//criando o arquivo se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpen (Disk *d, const char *path) {
	if(d == NULL || path == NULL) return -1;
	
	Inode* inodeRoot = inodeLoad(ID_INODE_DEFAULT, d);
	unsigned char superBlock[DISK_SECTORDATASIZE] = {0};
	unsigned char filesname[DISK_SECTORDATASIZE] = {0};
	unsigned char aux[DISK_SECTORDATASIZE] = {0};
	int fileIndex = -1;
	int descriptorIndex = 0;

	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(fileDescriptor[i].isOpen) {
			descriptorIndex = i;
		}
	}

	diskReadSector(d,0,superBlock);
	char2ul(&superBlock[INDEX_BLOCK_ROOT], &diretoryRoot.block);
	
	// diskReadSector(d,diretoryRoot.block*8, &diretoryRoot.files);

	//verifica se o filename ja existe
	for(int i = 0; i < inodeGetRefCount(inodeRoot); i++) {
		if(strcmp(diretoryRoot.files[i]->name, path)) {
			fileDescriptor[descriptorIndex].name = path;
			fileDescriptor[descriptorIndex].descriptor = "r+"; //abre para leitura e escrita
			fileDescriptor[descriptorIndex].isOpen = 1;
			return descriptorIndex;
		}
	}

	//pega um inode criado
	fileDescriptor[descriptorIndex].inode = inodeLoad(inodeFindFreeInode(ID_INODE_DEFAULT,d),d);
	if(fileDescriptor[descriptorIndex].inode == NULL) return -1;
	
	// //pega o numero do bloco livre
	// int blockFree = _bitMapGetBlockFree(d);
	// if(blockFree == -1) return -1;

	// // seta o tipo para arquivo regular (0 para arquivo regular e 1 para diretório)
	// inodeSetFileType(fileDescriptor[descriptorIndex].inode, 0); 
	// int idBlock = inodeAddBlock(fileDescriptor[descriptorIndex].inode,blockFree); //cria um novo bloco na lista de blocos do inode
	// if(idBlock == -1) return -1;
	// _bitMapSetFreePerBusy(blockFree); //coloca o bloco como ocupado

	fileDescriptor[descriptorIndex].name = path;
	fileDescriptor[descriptorIndex].descriptor = "r+"; //abre para leitura e escrita
	fileDescriptor[descriptorIndex].isOpen = 1;

	return _addDiretoryEntry(d,path,fileDescriptor[descriptorIndex].inode);
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
