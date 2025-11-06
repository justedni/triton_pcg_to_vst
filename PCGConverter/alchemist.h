#pragma once

#include <string>
#include <cstdint>

typedef char str16[16];
typedef struct { char data[4]; } Quad;

int QuadCmp(Quad q1, Quad q2);

unsigned long fRead32(FILE* f);
unsigned long Read32(unsigned char* s);

typedef struct structChunk {
	Quad quad;
	unsigned long size;
	unsigned long headersize;
	unsigned char dataowner;
	unsigned char* data;
	unsigned long childcount;
	struct structChunk* childs;
} Chunk;

enum class EnumKorgModel : uint8_t {
	KORG_TRITON = 0,
	KORG_KARMA,
	KORG_TRITON_EXTREME,
	KORG_TRITON_LE
};

typedef struct {
	unsigned long recordsize;
	unsigned char* data;
} KorgItem;

typedef struct {
	Quad quad;
	unsigned long recordsize;
	unsigned char* data;
} KorgBlock;

struct KorgBank {
	Quad quad;
	unsigned long count;
	unsigned long recordsize;
	unsigned long bank;
	KorgItem** item;
};

typedef struct {
	unsigned long count;
	KorgBank** bank;
} KorgBanks;

struct KorgPCG {
	EnumKorgModel model;
	KorgBanks* Arpeggio, * Combination, * Drumkit, * MOSS, * Program, * Prophecy;
	KorgBlock* CSM1, * DIV1, * Global;
};

const Quad QUAD_NONE = { { ' ', ' ', ' ', ' ' } };
const Quad QUAD_PCG1 = { { 'P', 'C', 'G', '1' } };
const Quad QUAD_PRG1 = { { 'P', 'R', 'G', '1' } };
const Quad QUAD_PBK1 = { { 'P', 'B', 'K', '1' } };
const Quad QUAD_MBK1 = { { 'M', 'B', 'K', '1' } };
const Quad QUAD_CMB1 = { { 'C', 'M', 'B', '1' } };
const Quad QUAD_CBK1 = { { 'C', 'B', 'K', '1' } };
const Quad QUAD_DKT1 = { { 'D', 'K', 'T', '1' } };
const Quad QUAD_DBK1 = { { 'D', 'B', 'K', '1' } };
const Quad QUAD_ARP1 = { { 'A', 'R', 'P', '1' } };
const Quad QUAD_ABK1 = { { 'A', 'B', 'K', '1' } };
const Quad QUAD_GLB1 = { { 'G', 'L', 'B', '1' } };
const Quad QUAD_CSM1 = { { 'C', 'S', 'M', '1' } }; /* Triton keyboard */
const Quad QUAD_DIV1 = { { 'D', 'I', 'V', '1' } }; /* Triton rack */

void InitChunk(Chunk* chunk);
void InitKorgItem(KorgItem* item, unsigned long recordsize);
void DeleteKorgItem(KorgItem* item);

void DestroyChunk(Chunk* chunk);

KorgPCG* CreateKorgPCG(EnumKorgModel model);
KorgBanks* CreateKorgBanks();
void AddKorgBank(KorgBanks* banks, KorgBank* bank);

void DeleteKorgBlock(KorgBlock* block);
void DeleteKorgBank(KorgBank* bank);
void DeleteKorgBanks(KorgBanks* banks);
void DeleteKorgPCG(KorgPCG* PCG);

void InitKorgBlock(KorgBlock* block, Quad quad, unsigned long recordsize);

KorgItem* CreateKorgItem(unsigned long recordsize, unsigned long size, const unsigned char* data);
KorgBlock* CreateKorgBlock(Quad quad, unsigned long recordsize, const unsigned char* data);
KorgBank* CreateKorgBank(Quad quad, unsigned long bank, unsigned long count, unsigned long recordsize, unsigned long size, const unsigned char* data);

KorgPCG* LoadTritonPCG(const char* file, EnumKorgModel& out_model);
