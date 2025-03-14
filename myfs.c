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
#define INDEX_SIZE_BITMAP 12 //index no superbloco para encontrar a quantidade de blocos de arquivos
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
	char name[MAX_FILE_LENGTH];
	unsigned char* descriptor;
	unsigned int isOpen; // 0 para fechado, 1 para aberto
} FileDescriptor;

typedef struct directoryFileEntry {
	char name[MAX_FILE_LENGTH];
	unsigned int numInode;
}DirectoryFileEntry;

typedef struct diretory {
	DirectoryFileEntry* files;
	int contRef;
} Directory;

typedef struct superblock {
	Inode* inodeRoot;
	unsigned char* bitMap;
	unsigned int totalBlocks;
	unsigned int blockSize;
	unsigned int sectorInit;
	unsigned int sizeBitMap;
	unsigned int blockRoot;
} SuperBlock;

SuperBlock superblock;
Directory directory;
FileDescriptor fileDescriptor[MAX_OPEN_FILES];

//**************************************************
// FUNÇÕES PRIVADAS - CRIADAS PELOS ALUNOS
//**************************************************

//função que retorna o numero de um bloco livre no disco
//retorna o numero de bloco caso encontre
//se não encontrar nenhum, retorna -1
int _bitMapGetBlockFree(Disk* d)
{
	//procura algum bloco livre, se sim retorna a soma do bloco root mais o i do bitmap
	for(int i = 0; i < superblock.sizeBitMap; i++) {
		if(!superblock.bitMap[i]){
			return superblock.blockRoot+i;
		} 
	}
	return -1;
}

//função que coloca o bloco dado como ocupado
void _bitMapSetFreePerBusy(int blockFree)
{
	superblock.bitMap[blockFree - superblock.blockRoot] = 1; 
}

//função que coloca o bloco dado como desocupado
void _bitMapSetBusyPerFree(int blockBusy)
{
	superblock.bitMap[blockBusy - superblock.blockRoot] = 0; 
}

//função que cria todos os inodes suportados no filesystema
int _initInode(Disk *d)
{
	for(int i = 1; i <= MAX_INODES; i++) {
		inodeCreate(i,d);
		// if(inodeCreate(i,d) == NULL) {
		// 	printf("\n retornou -1 no init inode");
		// 	return -1;	
	} 
	return 0;
}

//função que inicializa o diretório raiz
//lê os valores armazenados no superbloco 
//e coloca nas variáveis globais. 
//Tambem le o blockRoot e escreve no directory
int _initDirRoot(Disk* d)
{
	printf("etru dir\n");
	unsigned char diskSuperBlock[DISK_SECTORDATASIZE] = {0};
	
	if(!directory.contRef) {
		directory.files = malloc(sizeof(DirectoryFileEntry)*MAX_FILES);
		if(directory.files == NULL) return -1;
	}
	
	//lê o super blooc
	if(diskReadSector(d,0,diskSuperBlock) == -1) return -1;
	printf("leu superblokck\n");
	
	//passa os valores para as variáveis globais
	char2ul(&diskSuperBlock[INDEX_TOTALBLOCKS], &superblock.totalBlocks);
	char2ul(&diskSuperBlock[INDEX_BLOCKSIZE], &superblock.blockSize);
	char2ul(&diskSuperBlock[INDEX_SECTOR_INIT], &superblock.sectorInit);
	char2ul(&diskSuperBlock[INDEX_SIZE_BITMAP], &superblock.sizeBitMap);
	char2ul(&diskSuperBlock[INDEX_BLOCK_ROOT], &superblock.blockRoot);
	printf("transformou dados\n");
	
	superblock.bitMap = malloc(superblock.sizeBitMap);
	memcpy(superblock.bitMap, &diskSuperBlock[INDEX_BITMAP], superblock.sizeBitMap);
	printf("copiou bitmap\n");
	
	superblock.inodeRoot = inodeLoad(ID_INODE_DEFAULT, d);
	printf("leu inode root\n");
	
	//lê o root
	unsigned char* diskSectorRoot = malloc(DISK_SECTORDATASIZE * (superblock.blockSize/DISK_SECTORDATASIZE));
	memset(diskSectorRoot,0,sizeof(diskSectorRoot));
	
	for(int i = 0; i < superblock.blockSize/DISK_SECTORDATASIZE; i++) {
		if(diskReadSector(d,superblock.sectorInit-1+i,diskSectorRoot) == -1) return -1;
	}

	//faz o split dos dados lidos e armazena na variável global
	char* token = strtok(diskSectorRoot, "/,");
	int x = 0;
	while(token != NULL) {		
		if((*token >= 48) && (*token <= 57)) { // verifica se a string é numérica
			directory.files[x/2].numInode = *token - '0';
		} else {
			strcpy(directory.files[x/2].name, token);
		}

		token = strtok(NULL, "/,");

		x += 1;
	}
	
	directory.contRef = x/2; // porque o x sempre será o dobro da quantidade de dados no diretorio

	return 0;

}

//função que cria o diretório raiz
//cria um bloco de dados correspondente ao 
//primeiro bloco livre do disco e salva o inode
//retorna o numero do bloco caso de sucesso
//e -1 caso contrário
int _createDirRoot(Disk* d)
{
	directory.contRef = 0;//(apagar talvez)

	//pega o id do inode default e seta ele como arquivo de diretório
	inodeSetFileType(inodeLoad(ID_INODE_DEFAULT, d), 1); // 0 para arquivo regular, 1 para diretório
	
	//cria um novo bloco na lista de blocos do inode
	int idBlock = inodeAddBlock(inodeLoad(ID_INODE_DEFAULT, d),0); //colocando o bloco inicial como sendo o zero, tudo o que vem antes está sendo contado somente como setores
	printf("id do block root: %d\n", idBlock);
	if(idBlock == -1) return -1;
	
	// pega o bloco correspondente ao id do bloco criado
	unsigned int blockRoot =  inodeGetBlockAddr(inodeLoad(ID_INODE_DEFAULT, d),idBlock); 
	printf("num do block root: %d\n", superblock.blockRoot);
	if(inodeSave(inodeLoad(ID_INODE_DEFAULT, d)) == -1) return -1;

	return blockRoot;

}

//função que cria uma nova entrada 
//no diretório raiz. retorna -1 caso de mal sucedico
//e 0 caso feito com sucesso
int _addDiretoryEntry(Disk* d, const char* filename, Inode* inode)
{
	char aux[DISK_SECTORDATASIZE] = {0};
	unsigned char aux2[DISK_SECTORDATASIZE] = {0};
	
	for(int i = 0; i < directory.contRef; i++) {
		if(!strcmp(directory.files[i].name, filename)) return -1; //ja existe um arquivo com esse nome
	}
		
	strcpy(directory.files[directory.contRef].name, filename);
	directory.files[directory.contRef].numInode = inodeGetNumber(inode);
	printf("name depois de adicionado: %s\n", directory.files[directory.contRef].name);

	int sizeEntry = snprintf(NULL,0, "%d,%s/", directory.files[directory.contRef].numInode, directory.files[directory.contRef].name);
	for(int i = 0; i <= directory.contRef; i++) {
		char* dataEntry = malloc(sizeEntry + 1); // soma mais um pq o snprintf não conta o \0 no final da string
		sprintf(dataEntry, "%d,%s/", directory.files[i].numInode, directory.files[i].name);
		// asprintf(&dataEntry, "%d,%s/", directory.files[directory.contRef].numInode, directory.files[directory.contRef].name);
		printf("data entry: %s", dataEntry);
		strcat(aux, dataEntry);
		free(dataEntry);
	}
	
	printf("aux: %s\n", aux);
	
	int sectorAdd = (superblock.blockSize/DISK_SECTORDATASIZE)*superblock.blockRoot + superblock.sectorInit;
	printf("setor add: %d\n", sectorAdd);


	if(diskWriteSector(d,sectorAdd,aux) == -1) return -1;

	// printf("escreveu\n");
	// if(diskReadSector(d,sectorAdd,aux2) == -1) return -1;
	
	// printf("string lida: %s\n", aux2);
	// // for(int i = 0; i < 10; i++) {
	// // 	printf("leu3\n");
	// // 	printf("teste: 0x%02X\n", aux[i]);
		
	// // }

	// // diskGetSize

	directory.contRef += 1;
	
	return 0;

}

//**************************************************
// FUNÇÕES PUBLICAS
//**************************************************


//Funcao para verificacao se o sistema de arquivos está ocioso, ou seja,
//se nao ha quisquer descritores de arquivos em uso atualmente. Retorna
//um positivo se ocioso ou, caso contrario, 0.
int myFSIsIdle (Disk *d) {
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(fileDescriptor[i].isOpen) {
			return 0;
		}
	}

	return 1;
}

//Funcao para formatacao de um disco com o novo sistema de arquivos
//com tamanho de blocos igual a blockSize. Retorna o numero total de
//blocos disponiveis no disco, se formatado com sucesso. Caso contrario,
//retorna -1.
int myFSFormat (Disk *d, unsigned int blockSize) {
	if(d != NULL) {
		unsigned char diskSuperBlock[DISK_SECTORDATASIZE] = {0};
		// unsigned char aux[DISK_SECTORDATASIZE] = {0};
		unsigned char clearSectores[DISK_SECTORDATASIZE] = {0};
		
		//limpa os setores
		for(int i = 0; i < diskGetNumSectors(d); i++) {
			if(diskWriteSector(d,i,clearSectores) == -1) return -1;
		}

		//armazena o valor total de blocos no super bloco
		unsigned int totalBlocks = diskGetSize(d) / blockSize;
		ul2char(totalBlocks, &diskSuperBlock[INDEX_TOTALBLOCKS]);
		
		//armazena o valor do blocksize no super bloco 
		ul2char(blockSize, &diskSuperBlock[INDEX_BLOCKSIZE]);

		//calcula quantos setores será necessário para armazenar todos os inodes e qual o primeiro setor livre
		unsigned int sizeInodeSpace = MAX_INODES/inodeNumInodesPerSector(); //1024/8 = 128 setores
		unsigned int sectorInit = sizeInodeSpace+NUM_SECTOR_INIT_INODE; // 128 + 2 = 130
		ul2char(sectorInit, &diskSuperBlock[INDEX_SECTOR_INIT]);
		
		//cria todos os inodes e armazena no disco
		if(_initInode(d) == -1) return -1;
		//cria o diretorio raiz e um bloco de dados e armazena no superbloco o bloco do diretorio raiz
		unsigned int blockRoot = _createDirRoot(d);
		if(blockRoot == -1) return -1;
		ul2char(blockRoot, &diskSuperBlock[INDEX_BLOCK_ROOT]);
		
		//armazena o tamanho bitmap dos blocos livres no superbloco | 256 blocos - 128/(1024/512) = 256 - 64 = 192 blocos
																			//    128+2/(1024/512) = 256 - 65 = 191 blocos
		unsigned int tamBitMap = (totalBlocks - ((sizeInodeSpace+NUM_SECTOR_INIT_INODE)/(blockSize/DISK_SECTORDATASIZE))); //calcula quantos bits serao necessarios para armazenar os blocos livres
		ul2char(tamBitMap, &diskSuperBlock[INDEX_SIZE_BITMAP]);
		
		unsigned char* bitMap = malloc(tamBitMap);
		memset(bitMap,0,tamBitMap);
		bitMap[0] = 1; // marca como ocupado o primeiro bloco correspondente ao diretório raiz 
		memcpy(&diskSuperBlock[INDEX_BITMAP], bitMap, tamBitMap); //copia os dados do bitmap pro superblock
		
		//escreve no setor zero o superbloco
		if(diskWriteSector(d,0,diskSuperBlock) == -1) return -1;
		
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

	if(superblock.bitMap == NULL) {
		printf("inicializou root");
		if(_initDirRoot(d) == -1) return -1;
	}

	printf("iniciou dir\n");
	
	int descriptorIndex = -1;
	int auxVerify = 0; //0 o arquivo não existe, 1 existe
	
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(!fileDescriptor[i].isOpen) {
			descriptorIndex = i;
			break;
		}
	}

	//verifica se o arquivo ja está aberto(arrumar)
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if(!strcmp(fileDescriptor[i].name, path)) return -1;
	}
		
	//verifica se o filename ja existe
	for(int i = 0; i < directory.contRef; i++) {
		printf("name: %s\n", directory.files[i].name);
		printf("path: %s\n", path);
		if(!strcmp(directory.files[i].name, path)) {
			printf("entrou id igual\n");
			auxVerify = 1;
			break;
		}
	}

	//caso não tenha esse arquivo, então cria
	if(!auxVerify) {
		//pega um inode criado
		fileDescriptor[descriptorIndex].inode = inodeLoad(inodeFindFreeInode(ID_INODE_DEFAULT,d),d);
		printf("inode pegado: %s\n", inodeGetNumber(fileDescriptor[descriptorIndex].inode));
		if(fileDescriptor[descriptorIndex].inode == NULL) return -1;
	
		printf("\npegou o inode\n");
		if(_addDiretoryEntry(d,path,fileDescriptor[descriptorIndex].inode) == -1) return -1;
	}
	
	
	// //pega o numero do bloco livre
	// int blockFree = _bitMapGetBlockFree(d);
	// if(blockFree == -1) return -1;

	// // seta o tipo para arquivo regular (0 para arquivo regular e 1 para diretório)
	// inodeSetFileType(fileDescriptor[descriptorIndex].inode, 0); 
	// int idBlock = inodeAddBlock(fileDescriptor[descriptorIndex].inode,blockFree); //cria um novo bloco na lista de blocos do inode
	// if(idBlock == -1) return -1;
	// _bitMapSetFreePerBusy(blockFree); //coloca o bloco como ocupado
	
	strcpy(fileDescriptor[descriptorIndex].name, path);
	fileDescriptor[descriptorIndex].descriptor = "r+"; //abre para leitura e escrita
	fileDescriptor[descriptorIndex].isOpen = 1;
	
	return descriptorIndex+1;
}
	
//Funcao para a leitura de um arquivo, a partir de um descritor de
//arquivo existente. Os dados lidos sao copiados para buf e terao
//tamanho maximo de nbytes. Retorna o numero de bytes efetivamente
//lidos em caso de sucesso ou -1, caso contrario.
int myFSRead (int fd, char *buf, unsigned int nbytes) {
	// if (fd < 0 || fd >= MAX_OPEN_FILES || buf == NULL){
	// 	return -1; // parametro invalido
	// }

	// if (!fileDescriptor[fd].isOpen){
	// 	return -1; // arquivo não aberto
	// }

	// unsigned long sectorAddr = inodeGetBlockAddr(fileDescriptor[fd].inode, 0);
	// unsigned char sectorData[DISK_SECTORDATASIZE];

	// if (d == NULL){
	// 	return -1; // disco não está montado
	// }

	// if (diskReadSector(d, sectorAddr, sectorData) != 0){
	// 	return -1; // Falha na leitura do setor
	// }

	// strncpy(buf, sectorData, nbytes);
	// return nbytes; // Concluído!
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
	if(fd <= 0) return -1;

	strcpy(fileDescriptor[fd-1].name, "");
	fileDescriptor[fd-1].descriptor = "";
	fileDescriptor[fd-1].inode = NULL;
	fileDescriptor[fd-1].isOpen = 0;

	return 0;
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
