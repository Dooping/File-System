
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 6
#define FREE 					0
#define USED					1


struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
};

struct fs_superblock superBlock;		//superbloco
int *blockMap;		//mapa de blobos livres e ocupados

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int inode_load(int inumber, struct fs_inode *inode){	//carregar um inode para memoria
	int nblock, entry;
	union fs_block inodes;

	nblock = inumber/INODES_PER_BLOCK;
	entry = inumber%INODES_PER_BLOCK;
	nblock++;	//saltar o superblock

	disk_read(nblock,inodes.data);
	
	
	*inode = inodes.inode[entry];
	return 0;
}

int inode_save(int inumber, struct fs_inode *inode){	//guardar o inode que esta na memoria para o disco
	int nblock, entry;
	union fs_block inodes;

	nblock = inumber/INODES_PER_BLOCK;
	entry = inumber%INODES_PER_BLOCK;
	nblock++;	//saltar o superblock

	disk_read(nblock,inodes.data);

	inodes.inode[entry] = *inode;

	disk_write(nblock,inodes.data);
	
	return 0;
}

int fs_format()	//formatar um disco
{
	union fs_block block;
	int i,j,ninodeblocks;
	if (superBlock.magic == 0){	//disco nao montado
		disk_read(0,block.data);
		block.super.magic = FS_MAGIC;
		block.super.nblocks = disk_size();
		block.super.ninodeblocks = 0.1*block.super.nblocks;
		if (block.super.nblocks%10)
			block.super.ninodeblocks++;
		block.super.ninodes=block.super.ninodeblocks*INODES_PER_BLOCK;
		ninodeblocks = block.super.ninodeblocks;
		disk_write(0,block.data);	//escrever o novo superbloco

		for (i = 1; i<=ninodeblocks; i++){		//percorrer todos os blocos de inodes
			for(j=0; j<INODES_PER_BLOCK;j++)		//percorrer todos os inodes no bloco
				block.inode[j].isvalid = 0;
			disk_write(i,block.data);
		}
		return 1;
	}
	else
		return 0;
}

void fs_debug()	//imprime o conteudo do disco
{
	union fs_block block;
	int ninodes, ninodeblocks, i, j, k;

	disk_read(0,block.data);

	printf("superblock:\n");
	if(block.super.magic ==FS_MAGIC)
		printf("    magic number is valid\n");
	else
		printf("    magic number is invalid\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
	ninodeblocks = block.super.ninodeblocks;
	ninodes = block.super.ninodes;

	for(i = 0; i<ninodeblocks; i++){		//percorre todos os blocos de inodes
		disk_read(1+i,block.data);
		for(j = 0; j<ninodes && j<INODES_PER_BLOCK;j++)	//percorre todos os inodes do bloco
		if(block.inode[j].isvalid>0)	//imprime apenas os validos
		{
			printf("inode %d\n",(i*INODES_PER_BLOCK)+j);
			printf("    size: %d bytes\n",block.inode[j].size);
			printf("    Blocks:");
			for(k=0;k<POINTERS_PER_INODE;k++)	//percorre todos os directs
				if (block.inode[j].direct[k]>0)
					printf(" %d",block.inode[j].direct[k]);
			printf("\n");
		}
	}
}

int fs_mount()		//monta o disco, copia o superblock para memoria
{
	union fs_block block;
	int size, i, j, k;

	disk_read(0,block.data);
	if (block.super.magic == FS_MAGIC){
		superBlock = block.super;
		blockMap = (int*)malloc(block.super.nblocks*sizeof(int));	//alocar memoria para a tabela de blocos livres/ocupados
		for (i = 0;i<=block.super.ninodeblocks;i++)		//ocupar os blockos referentes ao superbloco e blocos de inodes
			blockMap[i] = USED;

		for(i = 1; i<=superBlock.ninodeblocks; i++){			//percorrer todos os blocos de inodes
			disk_read(i,block.data);
			for(j = 0; j<superBlock.ninodes && j<INODES_PER_BLOCK;j++){	//percorrer todos os inodes do bloco
				size = block.inode[j].size/DISK_BLOCK_SIZE;
				if ((block.inode[j].size%DISK_BLOCK_SIZE)>0)
					size++;
				if (block.inode[j].isvalid)
				for(k=0;k<size;k++)
					blockMap[block.inode[j].direct[k]]=USED;		//marcar o bloco como ocupado na tabela de blocos livres/ocupados
			}				
		}
		return 1;
	}
	else
		return 0;
}

int fs_create()		//encontra o primeiro inode livre e marca-o como valido
{
	union fs_block block;
	int idx = -1, i = 1, j = 0, k;

	if (superBlock.magic != FS_MAGIC)
		return -1;
	while (idx == -1 && i <= superBlock.ninodeblocks){
		disk_read(i,block.data);
		while (idx == -1 && j < INODES_PER_BLOCK){
			if(block.inode[j].isvalid == 0){		//marcar o inode como valido
				idx = (i-1)*INODES_PER_BLOCK + j;
				block.inode[j].isvalid = 1;
				block.inode[j].size = 0;
				for (k = 0;k<POINTERS_PER_INODE;k++)	//inicializa a tabla de endereços directos
					block.inode[j].direct[k] = 0;
			}
			j++;
		}
		if (idx>=0)		//se criou um inode guarda o bloco correspondente
			disk_write(i,block.data);
		i++;
	}
	return idx;		//devolver o indice do novo inode
}

int fs_delete( int inumber )
{
	struct fs_inode ix;
	int i, dim_direct;

	if (superBlock.magic != FS_MAGIC)
		return 0;
	if (inumber < superBlock.ninodes){
		inode_load(inumber, &ix);
		if (ix.isvalid ==0)
			return 0;
		else{
			ix.isvalid = 0;
			inode_save(inumber, &ix);
			dim_direct = ix.size/DISK_BLOCK_SIZE;
			if (ix.size%DISK_BLOCK_SIZE > 0)
				dim_direct++;
			for (i= 0;i<dim_direct; i++)
				if(ix.direct[i]!=0)
					blockMap[ix.direct[i]]=FREE;
		}
		return 1;
	}
	return 0;
}

int fs_getsize( int inumber )		//devolve o tamanho dos dados pertencentes ao inode
{
	struct fs_inode ix;
	if (superBlock.magic != FS_MAGIC)
		return -1;
	if (inumber < superBlock.ninodes){
		inode_load(inumber, &ix);		//carrega o inode para memoria
		if (ix.isvalid)
			return ix.size;
	}
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )	//tenta ler dados do disco para um buffer dado
{
	struct fs_inode iy;
	char dataR[DISK_BLOCK_SIZE];
	int nbytesToRead, nbytes, currentOffset, offsetBlock, bytesTransfered;

	if (superBlock.magic != FS_MAGIC)		//disco nao montado
		return 0;
	if ( inumber > superBlock.ninodes)		//inode nao existente
		return -1;
	inode_load(inumber, &iy);
	if (iy.isvalid == 0)							//inode nao criado
		return -2;
	if (iy.size < offset)						//tentar ler de uma posicao invalida
		return -3;

	if (iy.size < offset+length)				//tentar ler para alem do fim do ficheiro
		nbytesToRead = iy.size - offset;
	else 
		nbytesToRead = length;

	currentOffset = offset;
	bytesTransfered = 0;
	while (nbytesToRead > 0){ //enquanto houver dados pra ler
		disk_read(iy.direct[currentOffset/DISK_BLOCK_SIZE],dataR);
		offsetBlock = currentOffset % DISK_BLOCK_SIZE;
		nbytes = DISK_BLOCK_SIZE - offsetBlock;
		if(nbytes > nbytesToRead)
			nbytes = nbytesToRead;
		bcopy(dataR + offsetBlock,data + bytesTransfered,nbytes);
		bytesTransfered += nbytes;
		nbytesToRead -= nbytes;
		currentOffset += nbytes;
	}
	return bytesTransfered;
}

/* funcao que encontra um bloco livre no mapa de blocos */
int searchFreeBlock(){		
	int i = 1, n = -1;
	while (i<superBlock.nblocks && n==-1)
		if (blockMap[i])
			i++;
		else
			n=i;

	return n;
}

int fs_write( int inumber, const char *data, int length, int offset )	//tenta escrever do buffer dado para o disco
{
	struct fs_inode iy;
	char dataW[DISK_BLOCK_SIZE];
	int nbytesToWrite, nbytes, currentOffset, offsetBlock, bytesTransfered, newBlock = -1, i;

	if (superBlock.magic != FS_MAGIC)		//disco nao montado
		return 0;
	if ( inumber > superBlock.ninodes)		//inode nao existente
		return -1;
	inode_load(inumber, &iy);
	if (iy.isvalid == 0)							//inode nao criado
		return -2;
	if (iy.size < offset)						//tentar escrever depois do final do ficheiro
		return -3;

	
	nbytesToWrite = length;

	currentOffset = offset;
	bytesTransfered = 0;
	while (nbytesToWrite > 0){		//enquanto houver dados para escrever
		if (currentOffset/DISK_BLOCK_SIZE >= POINTERS_PER_INODE)	//tamanho do ficheiro maior que o permitido
			break;
		if (iy.direct[currentOffset/DISK_BLOCK_SIZE] > 0){		//inode ja tem blocos associados
			disk_read(iy.direct[currentOffset/DISK_BLOCK_SIZE],dataW);
			newBlock = iy.direct[currentOffset/DISK_BLOCK_SIZE];
		}
		else
			newBlock = searchFreeBlock();
		if (newBlock == -1)	//disco cheio
			break;
		else{
			iy.direct[currentOffset/DISK_BLOCK_SIZE] = newBlock;
			blockMap[newBlock] = USED;
		}
		offsetBlock = bytesTransfered % DISK_BLOCK_SIZE;
		nbytes = DISK_BLOCK_SIZE - offsetBlock;
		if(nbytes > nbytesToWrite)
			nbytes = nbytesToWrite;
		bcopy(data + currentOffset,dataW + offsetBlock,nbytes);
		disk_write(iy.direct[currentOffset/DISK_BLOCK_SIZE], dataW);
		
		bytesTransfered += nbytes;
		nbytesToWrite -= nbytes;
		currentOffset += nbytes;
	}
	for (i=currentOffset/DISK_BLOCK_SIZE +1; i<POINTERS_PER_INODE;i++)	//libertar os blocos restantes
		if (iy.direct[i]>0){
			blockMap[iy.direct[i]] = FREE;
			iy.direct[i] = 0;
		}
	iy.size = bytesTransfered + offset;
	inode_save(inumber, &iy);
	return bytesTransfered;
}