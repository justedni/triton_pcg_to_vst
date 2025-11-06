#include "alchemist.h"

#include <cstring>

constexpr const int kKorgHeaderSize = 16;
unsigned char TritonPCGHeader[kKorgHeaderSize]			= { 'K', 'O', 'R', 'G', 0x50, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char KarmaPCGHeader[kKorgHeaderSize]			= { 'K', 'O', 'R', 'G', 0x5D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char TritonLePCGHeader[kKorgHeaderSize]		= { 'K', 'O', 'R', 'G', 0x63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char TritonExtremePCGHeader[kKorgHeaderSize]	= { 'K', 'O', 'R', 'G', 0x50, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0 };

int QuadCmp(Quad q1, Quad q2) {
	return memcmp(q1.data, q2.data, 4);
}

unsigned long fRead32(FILE* f) {
	unsigned long val;
	val = fgetc(f) << 24;
	val |= fgetc(f) << 16;
	val |= fgetc(f) << 8;
	val |= fgetc(f);
	return val;
}

unsigned long Read32(unsigned char* s) {
	unsigned long ret = *s++ << 24;
	ret |= *s++ << 16;
	ret |= *s++ << 8;
	return ret |= *s;
}

void InitChunk(Chunk* chunk) {
	unsigned char* ptr;
	unsigned long l, size, count;
	unsigned char ok;
	Chunk* c;

	if (chunk) {
		l = chunk->size;
		ptr = chunk->data;
		ok = 1;
		count = 0;
		while (ok && l) {
			ok = 0;
			if (l > 8) {
				ptr += 4;
				l -= 4;
				if (l > 4) {
					size = Read32(ptr);
					ptr += 4;
					l -= 4;
					if (l >= size) {
						l -= size;
						ptr += size;
						count++;
						ok = 1;
					}
				}
			}
		}
		if (ok && !l) {
			chunk->childcount = count;
			chunk->childs = (Chunk*)calloc(count, sizeof(Chunk));
			ptr = chunk->data;
			for (l = 0; l < chunk->childcount; l++) {
				c = &chunk->childs[l];
				c->quad = *((Quad*)ptr);
				ptr += 4;
				c->size = Read32(ptr);
				ptr += 4;
				c->data = ptr;
				ptr += c->size;
				InitChunk(c);
				c->size = c->size;
				c->headersize = 0;
			}
		}
	}
}

void InitKorgItem(KorgItem* item, unsigned long recordsize) {
	if (item) {
		item->recordsize = recordsize;
		item->data = (unsigned char*)malloc(recordsize);
	}
}

void DeleteKorgItem(KorgItem* item) {
	if (item) {
		if (item->data) {
			free(item->data);
			item->data = NULL;
		}
		free(item);
		item = NULL;
	}
}

void DestroyChunk(Chunk* chunk) {
	if (chunk) {
		if (chunk->childcount && chunk->childs) {
			unsigned long l;
			Chunk* child = chunk->childs;
			for (l = 0; l < chunk->childcount; l++, child++)
				DestroyChunk(child);
			free(chunk->childs);
			chunk->childs = NULL;
		}
		if (chunk->dataowner && chunk->data)
			free(chunk->data);
		chunk->data = NULL;
	}
}

KorgPCG* CreateKorgPCG(EnumKorgModel model) {
	KorgPCG* newpcg = (KorgPCG*)calloc(1, sizeof(KorgPCG));
	if (newpcg)
		newpcg->model = model;
	return newpcg;
}

KorgBanks* CreateKorgBanks() {
	return (KorgBanks*)calloc(1, sizeof(KorgBanks));
}

void AddKorgBank(KorgBanks* banks, KorgBank* bank) {
	/* don't duplicate */
	if (banks && bank) {
		banks->bank = (KorgBank**)realloc(banks->bank, (banks->count + 1) * sizeof(KorgBank*));
		banks->bank[banks->count++] = bank;
	}
}

void DeleteKorgBlock(KorgBlock* block) {
	if (block) {
		if (block->data) {
			free(block->data);
			block->data = NULL;
		}
		free(block);
		block = NULL;
	}
}

void DeleteKorgBank(KorgBank* bank) {
	if (bank) {
		unsigned long l;
		for (l = 0; l < bank->count; l++)
			DeleteKorgItem(bank->item[l]);
		free(bank->item);
		bank->item = NULL;
		free(bank);
		bank = NULL;
	}
}

void DeleteKorgBanks(KorgBanks* banks) {
	if (banks) {
		unsigned long l;
		for (l = 0; l < banks->count; l++)
			DeleteKorgBank(banks->bank[l]);
		free(banks->bank);
		banks->bank = NULL;
		free(banks);
		banks = NULL;
	}
}

void DeleteKorgPCG(KorgPCG* PCG) {
	if (PCG) {
		DeleteKorgBanks(PCG->Arpeggio);
		DeleteKorgBanks(PCG->Combination);
		DeleteKorgBanks(PCG->Drumkit);
		DeleteKorgBanks(PCG->MOSS);
		DeleteKorgBanks(PCG->Program);
		DeleteKorgBanks(PCG->Prophecy);
		DeleteKorgBlock(PCG->CSM1);
		DeleteKorgBlock(PCG->DIV1);
		DeleteKorgBlock(PCG->Global);
		free(PCG);
		PCG = NULL;
	}
}

KorgItem* CreateKorgItem(unsigned long recordsize, unsigned long size, const unsigned char* data) {
	KorgItem* newitem = (KorgItem*)malloc(sizeof(KorgItem));
	InitKorgItem(newitem, recordsize);
	if (newitem && newitem->data) {
		if (size && data) {
			if (size >= recordsize)
				memcpy(newitem->data, data, recordsize);
			else {
				memcpy(newitem->data, data, size);
				if (size && (size < recordsize))
					memset(newitem->data + size, 0, recordsize - size);
			}
		}
	}
	else {
		DeleteKorgItem(newitem);
		newitem = 0;
	}
	return newitem;
}

void InitKorgBlock(KorgBlock* block, Quad quad, unsigned long recordsize) {
	if (block) {
		block->data = (unsigned char*)malloc(recordsize);
		block->quad = quad;
		block->recordsize = recordsize;
	}
}

KorgBlock* CreateKorgBlock(Quad quad, unsigned long recordsize, const unsigned char* data) {
	KorgBlock* newblock = (KorgBlock*)malloc(sizeof(KorgBlock));
	InitKorgBlock(newblock, quad, recordsize);
	if (newblock && newblock->data) {
		if (data)
			memcpy(newblock->data, data, recordsize);
	}
	else {
		DeleteKorgBlock(newblock);
		newblock = 0;
	}
	return newblock;
}

KorgBank* CreateKorgBank(Quad quad, unsigned long bank, unsigned long count, unsigned long recordsize, unsigned long size, const unsigned char* data) {
	unsigned long l;
	KorgBank* newbank = (KorgBank*)malloc(sizeof(KorgBank));
	if (newbank) {
		if ((newbank->item = (KorgItem**)calloc(count, sizeof(KorgItem*))) != NULL) {
			newbank->quad = quad;
			newbank->count = count;
			newbank->recordsize = recordsize;
			newbank->bank = bank;
			for (l = 0; l < count; l++) {
				newbank->item[l] = CreateKorgItem(recordsize, size, data);
				if (size > recordsize)
					size -= recordsize;
				else
					size = 0;
				data += recordsize;
			}
		}
		else {
			free(newbank);
			newbank = NULL;
			return NULL;
		}
	}
	return newbank;
}


KorgPCG* LoadTritonPCG(const char* file, EnumKorgModel& out_model) {
	KorgPCG* PCG = NULL;
	FILE* infile;
	Chunk PCGChunk, * c1;
	unsigned long l, buflen, l1;

	auto success = infile = fopen(file, "rb");
	if (!infile) {
		fprintf(stderr, "File not found: \"%s\".\n", file);
		return NULL;
	}

	/* reading the header */
	unsigned char filehead[kKorgHeaderSize];
	fread(filehead, kKorgHeaderSize, 1, infile);

	if (memcmp(filehead, TritonPCGHeader, kKorgHeaderSize) == 0)
	{
		out_model = EnumKorgModel::KORG_TRITON;
	}
	else if (memcmp(filehead, TritonExtremePCGHeader, kKorgHeaderSize) == 0)
	{
		out_model = EnumKorgModel::KORG_TRITON_EXTREME;
	}
	else if (memcmp(filehead, KarmaPCGHeader, kKorgHeaderSize) == 0)
	{
		out_model = EnumKorgModel::KORG_KARMA;
	}
	else if (memcmp(filehead, TritonLePCGHeader, kKorgHeaderSize) == 0)
	{
		out_model = EnumKorgModel::KORG_TRITON_LE;
	}
	else
	{
		fprintf(stderr, "Input file \"%s\" is not a valid Triton PCG (bad header).\n", file);
		fclose(infile);
		return NULL;
	}

	memset(&PCGChunk, 0, sizeof(Chunk));

	fread(&PCGChunk.quad, sizeof(Quad), 1, infile);
	if (QuadCmp(PCGChunk.quad, QUAD_PCG1)) {
		fprintf(stderr, "Input file \"%s\" is not a valid PCG (bad root chunk).\n", file);
		fclose(infile);
		return NULL;
	}

	PCGChunk.size = fRead32(infile);
	PCGChunk.size = PCGChunk.size;

	l = ftell(infile);
	fseek(infile, 0, SEEK_END);
	buflen = ftell(infile) - l;
	fseek(infile, l, SEEK_SET);

	if (PCGChunk.size != buflen) {
		fprintf(stderr, "Input file \"%s\" is not a valid PCG (incorrect size).\n", file);
		fclose(infile);
		return NULL;
	}

	PCGChunk.data = (unsigned char*)malloc(buflen);
	PCGChunk.dataowner = 1;
	fread(PCGChunk.data, PCGChunk.size, 1, infile);

	InitChunk(&PCGChunk);

	if (!PCGChunk.childcount) {
		fprintf(stderr, "Input file \"%s\" is not a valid PCG (empty PCG?).\n", file);
		DestroyChunk(&PCGChunk);
		fclose(infile);
		return NULL;
	}

	PCG = CreateKorgPCG(out_model);

	/* Parsing PCG1 */
	for (l1 = 0, c1 = PCGChunk.childs; l1 < PCGChunk.childcount; l1++, c1++) {
		if (!QuadCmp(c1->quad, QUAD_PRG1) || !QuadCmp(c1->quad, QUAD_CMB1) || !QuadCmp(c1->quad, QUAD_DKT1) || !QuadCmp(c1->quad, QUAD_ARP1)) {
			if (c1->childcount) {
				/* PRG1, CMB1, DKT1, ARP1: bank containers */
				Chunk* c2;
				unsigned long l2;
				for (l2 = 0, c2 = c1->childs; l2 < c1->childcount; l2++, c2++) {
					/* reading each bank */
					KorgBanks** banksptr;
					if (!QuadCmp(c2->quad, QUAD_PBK1) || !QuadCmp(c2->quad, QUAD_MBK1) || !QuadCmp(c2->quad, QUAD_CBK1) || !QuadCmp(c2->quad, QUAD_DBK1) || !QuadCmp(c2->quad, QUAD_ABK1)) {
						/* PBK1, MBK1, CBK1, DBK1, ABK1: banks */
						unsigned long number, size, bank;
						if (!QuadCmp(c2->quad, QUAD_PBK1))
							banksptr = &PCG->Program;
						else if (!QuadCmp(c2->quad, QUAD_MBK1))
							banksptr = &PCG->MOSS;
						else if (!QuadCmp(c2->quad, QUAD_CBK1))
							banksptr = &PCG->Combination;
						else if (!QuadCmp(c2->quad, QUAD_DBK1))
							banksptr = &PCG->Drumkit;
						else /* if (!QuadCmp(c2->quad, QUAD_ABK1)) */
							banksptr = &PCG->Arpeggio;

						if (!*banksptr)
							*banksptr = CreateKorgBanks();

						number = Read32(c2->data);
						size = Read32(c2->data + 4);
						bank = Read32(c2->data + 8);
						AddKorgBank(*banksptr, CreateKorgBank(c2->quad, bank, number, size, c2->size - 12, c2->data + 12));
					}
					else
						fprintf(stderr, "[%c%c%c%c] Unknown QUAD \"%c%c%c%c\"\n", c1->quad.data[0], c1->quad.data[1], c1->quad.data[2], c1->quad.data[3], c2->quad.data[0], c2->quad.data[1], c2->quad.data[2], c2->quad.data[3]);
				}
			}
		}
		else if (!QuadCmp(c1->quad, QUAD_CSM1) || !QuadCmp(c1->quad, QUAD_DIV1) || !QuadCmp(c1->quad, QUAD_GLB1)) {
			if (!c1->childcount) {
				/* CSM1, DIV1, GLB1: 1 item blocks */
				KorgBlock** blockptr;
				if (!QuadCmp(c1->quad, QUAD_CSM1))
					blockptr = &PCG->CSM1;
				else if (!QuadCmp(c1->quad, QUAD_DIV1))
					blockptr = &PCG->DIV1;
				else /* if (!QuadCmp(c1->quad, QUAD_GLB1)) */
					blockptr = &PCG->Global;

				if (*blockptr)
					DeleteKorgBlock(*blockptr);

				*blockptr = CreateKorgBlock(c1->quad, c1->size, c1->data);
			}
		}
		else {
			fprintf(stderr, "BAD QUAD \"%c%c%c%c\"\n", c1->quad.data[0], c1->quad.data[1], c1->quad.data[2], c1->quad.data[3]);
		}
	}

	DestroyChunk(&PCGChunk);
	fclose(infile);

	return PCG;
}
