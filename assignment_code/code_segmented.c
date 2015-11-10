/* Initial version of cavity detector */

/* Not correct for boundaries */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define N 2144
#define M 2144

#define IN_NAME "./../test_im_med.pgm"
#define OUT_NAME "out.pgm"
#define OUT_NAME1 "test.out1.pgm"	// only used for output of temporary images
#define OUT_NAME2 "test.out2.pgm"	// idem



#define DO_RECORD //to turn on/off

//Macros for access counting
//(usage: just take a line of code and wrap it in a DO() macro, manually entering the number of reads and writes for that line)
#ifdef DO_RECORD
#define RECORD(STAGE, READMEM, WRITEMEM, cmd) {reads[STAGE] += READMEM; writes[STAGE] += WRITEMEM; cmd;}
#else
#define RECORD(STAGE, READMEM, WRITEMEM, cmd) {cmd;}
#endif

#define GB 32 
#define NB 16

enum BUFFERS{
	IN_IMAGE = 0,
	OUT_IMAGE,
	G_IMAGE,
	C_IMAGE,
	TMP,
	TMP2,
	GAUSS,
	SCR,
	SCR2,
	SCR3,
	SCR4,
	BUFFERS_MAX
};

#define DRAM_TIME 40
#define SRAM_TIME 2
int BUFTIMES[BUFFERS_MAX] = {
	DRAM_TIME,      //IN_IMAGE = 0
	DRAM_TIME,      //OUT_IMAGE,
	DRAM_TIME,      //G_IMAGE,
	DRAM_TIME,      //C_IMAGE,
	DRAM_TIME,      //TMP,
	DRAM_TIME,      //TMP2,
	DRAM_TIME,      //GAUSS,
	SRAM_TIME,      //SCR,
	SRAM_TIME,      //SCR2,
	SRAM_TIME,      //SCR3,
	SRAM_TIME,      //SCR4,
};

const char * BUFNAMES[BUFFERS_MAX] = {
	"IN_IMAGE",
	"OUT_IMAGE",
	"G_IMAGE",
	"C_IMAGE",
	"TMP",
	"TMP2",
	"GAUSS",
	"SCR",
	"SCR2",
	"SCR3",
	"SCR4"
};




#define TH 32

#define SCRSIZE (66)  //SCRATCHPAD 128*4= 512B
#define SCR2X (34) //SCRATCHPAD 64*2144*4= 548864B (gauss to edge)
#define SCR2Y (TH+65)
#define SCR3X (34) //SCRATCHPAD 64*2144*4= 548864B (edge to roots)
#define SCR3Y (TH+33)
#define SCR4X (65) //(in to gauss)
#define SCR4Y (TH+129)

int BUFSIZES[BUFFERS_MAX] = {
	M*N*4,      //IN_IMAGE = 0
	M*N*4,      //OUT_IMAGE,
	0,      //G_IMAGE,
	0,      //C_IMAGE,
	0,      //TMP,
	0,      //TMP2,
	(65)*4,      //GAUSS,
	SCRSIZE*4,      //SCR,
	SCR2X*SCR2Y*4,      //SCR2,
	SCR3X*SCR3Y*4,      //SCR3,
	SCR4X*SCR4Y*4      //SCR4,
};

#define DRAM_ENERGY 2000000 //femtojoule
#define SRAM_SIZE 1 //kB
#define SRAM2_SIZE 16 //kB
#define SRAM3_SIZE 16 //kB
#define SRAM4_SIZE 64 //kB
#define SRAM_ENERGY (SRAM_SIZE*1100+6200) //femtojoule
#define SRAM2_ENERGY (SRAM2_SIZE*1100+6200) //femtojoule
#define SRAM3_ENERGY (SRAM3_SIZE*1100+6200) //femtojoule
#define SRAM4_ENERGY (SRAM4_SIZE*1100+6200) //femtojoule

int BUFENERGIES[BUFFERS_MAX] = {
	DRAM_ENERGY,      //IN_IMAGE = 0
	DRAM_ENERGY,      //OUT_IMAGE,
	0,      //G_IMAGE,
	0,      //C_IMAGE,
	0,      //TMP,
	0,      //TMP2,
	SRAM_ENERGY,      //GAUSS,
	SRAM_ENERGY,      //SCR,
	SRAM2_ENERGY,      //SCR2,
	SRAM3_ENERGY,      //SCR3,
	SRAM4_ENERGY      //SCR4,
};

//globals
long long unsigned int writes[BUFFERS_MAX];
long long unsigned int reads[BUFFERS_MAX];

#if 32 <= 3
int Gauss[]={99,68,35,10};
#elif 32 == 4
int Gauss[]={99,82,46,18,5};
#elif 32 == 5
int Gauss[]={99,88,61,33,14,5};
#elif 32 == 6
int Gauss[]={99,91,70,46,25,12,5};
#elif 32 == 7
int Gauss[]={99,93,77,56,36,21,10,5};
#elif 32 == 8
int Gauss[]={99,95,82,64,46,30,18,10,5};
#elif 32 == 32
int Gauss[]={99,99,98,97,95,92,89,86,82,78,73,69,64,60,55,50,46,41,37,33,30,26,23,20,18,15,13,11,10,8,7,6,5}; 
#elif 32==64
int Gauss[]={99,99,99,99,98,98,97,96,95,94,92,91,89,88,86,84,82,80,78,76,73,71,69,67,64,62,60,57,55,53,50,48,46,44,41,39,37,35,33,32,30,28,26,25,23,22,20,19,18,16,15,14,13,12,11,10,10,9,8,7,7,6,6,5,5};
#else
#error Dit mag niet!
#endif
//SCRATCHPAD 65

int getint(FILE *fp) /* adapted from "xv" source code */
{
	int c, i, firstchar, garbage;

	/* note:  if it sees a '#' character, all characters from there to end of
	line are appended to the comment string */

	/* skip forward to start of next number */
	c = getc(fp);
	while (1) {
		/* eat comments */
		if (c=='#') {
			/* if we're at a comment, read to end of line */
			char cmt[256], *sp;

			sp = cmt;  firstchar = 1;
			while (1) {
				c=getc(fp);
				if (firstchar && c == ' ') firstchar = 0;  /* lop off 1 sp after # */
				else {
					if (c == '\n' || c == EOF) break;
					if ((sp-cmt)<250) *sp++ = c;
				}
			}
			*sp++ = '\n';
			*sp   = '\0';
		}

		if (c==EOF) return 0;
		if (c>='0' && c<='9') break;   /* we've found what we were looking for */

		/* see if we are getting garbage (non-whitespace) */
		if (c!=' ' && c!='\t' && c!='\r' && c!='\n' && c!=',') garbage=1;

		c = getc(fp);
	}

	/* we're at the start of a number, continue until we hit a non-number */
	i = 0;
	while (1) {
		i = (i*10) + (c - '0');
		c = getc(fp);
		if (c==EOF) return i;
		if (c<'0' || c>'9') break;
	}
	return i;
}


void openfile(char *filename, FILE** finput)
{
	int x0, y0;
	char header[255];

	if ((*finput=fopen(filename,"rb"))==NULL) {
		fprintf(stderr,"Unable to open file %s for reading\n",filename);
		exit(-1);
	}

	fscanf(*finput,"%s",header);
	if (strcmp(header,"P2")!=0) {
		fprintf(stderr,"\nFile %s is not a valid ascii .pgm file (type P2)\n",
		filename);
		exit(-1);
	}

	x0=getint(*finput);
	y0=getint(*finput);

	if ((x0!=N) || (y0!=M)) {
		fprintf(stderr,"Image dimensions do not match: %ix%i expected\n", N, M);
		exit(-1);
	}

	getint(*finput); /* read and throw away the range info */
}


void read_image(char* filename, int image[N][M])
{
	long int inint;
	FILE* finput;
	int i,j;

	finput=NULL;
	openfile(filename,&finput);
	for (j=0; j<M; ++j)
	for (i=0; i<N; ++i) {
		if (fscanf(finput, "%li", &inint)==EOF) {
			fprintf(stderr,"Premature EOF\n");
			exit(-1);
		} else {
			image[i][j]= (int) inint;
		}
	}
	fclose(finput);
}


void write_image(char* filename, int image[N][M])
{
	FILE* foutput;
	int i,j;

	if ((foutput=fopen(filename,"wb"))==NULL) {
		fprintf(stderr,"Unable to open file %s for writing\n",filename);
		exit(-1);
	}

	fprintf(foutput,"P2\n");
	fprintf(foutput,"%d %d\n",N,M);
	fprintf(foutput,"%d\n",255);

	for (j=0; j<M; ++j) {
		for (i=0; i<N; ++i) {
			fprintf(foutput,"%3d ",image[i][j]);
			if (i%32==31) fprintf(foutput,"\n");
		}
		if (N%32!=0) fprintf(foutput,"\n");
	}
	fclose(foutput);
}



void GaussBlur (int image_in[N][M], int image_outg[N][M], int image_oute[N][M], int image_outr[N][M])
{  
	//step 1: SRAM buffer for gauss input reuse

	int scr[66]; //SCRATCHPAD ???
	static int scr2[SCR2X][SCR2Y];
	static int scr3[SCR3X][SCR3Y]; 
	static int scr4[SCR4X][SCR4Y];
	int x,y,k;
	int maxdiff,val;
	int x_offset,y_offset;
	int temp;

	int tot=0;
	for (k=-32; k<=32; ++k) RECORD(GAUSS,1,0, tot+=Gauss[abs(k)]);




	//START PROLOGUE
	for (x=0; x<SCR2X; ++x) for (y=0; y<SCR2Y; ++y) RECORD(SCR2,0,1, scr2[x][y]=0); //0-init
	for (x=0; x<SCR3X; ++x) for (y=0; y<SCR3Y; ++y) RECORD(SCR3,0,1, scr3[x][y]=0);  //0-init	
	for (y=0; y<SCRSIZE; ++y) RECORD(SCR,0,1, scr[y]=0);
	

	for (x=0; x<N; ++x) for (y=0; y<M; ++y) RECORD(OUT_IMAGE,0,1, image_outr[x][y]=0);
	
	for (x=0; x<SCR4X; ++x) for (y=0; y<129; ++y) {RECORD(SCR4,0,1, scr4[x][y%SCR4Y]=image_in[x][y]); RECORD(IN_IMAGE,1,0,;);}  //PROD scr4[0..64][0..128]
	for(y=32; y<=64; ++y){ //DEP scr4[0..64][32..64], PROD scr[32..64]
		temp=0;
		for (k=-32; k<=32; ++k) //SDEP scr4[0..64][y], SPROD scr[y]
		{RECORD(SCR4,1,0,;);
			RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
		RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
	}
	for(y=65; y<129; ++y) { //DEP scr[0..64][65..128], PROD scr2[32][32..M-66], PROD scr[65..M-33]
		temp=0;
		for(k=-32; k<=32; ++k) //SDEP scr4[0..64][y], SPROD scr[y]
		{RECORD(SCR4,1,0,;);
			RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
		RECORD(SCR,0,1, scr[y%SCRSIZE] = temp/tot);
		temp=0;
		for(k=-32; k<=32; ++k) //SDEP scr[y-65...y-1], SPROD scr2[32][y-33]
		{
			RECORD(SCR,1,0,;);
			RECORD(GAUSS,1,0, temp += scr[(y-33+k)%(SCRSIZE)] * Gauss[abs(k)]);}
		RECORD(SCR2,0,1, scr2[32%(SCR2X)][(y-33)%SCR2Y] =temp/tot);
	}

	for (x=33; x<50; ++x){
		for(y=0;y<129;y++) {RECORD(SCR4, 0,1, scr4[(x+32)%SCR4X][y%SCR4Y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[65..81][0..128]
		temp=0;
		for(y=32; y<=64; ++y){ //DEP scr4[1..65][32..64], PROD scr[32..64]
			for (k=-32; k<=32; ++k) //SDEP scr4[1..65][y], SPROD scr[y] 
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] = temp/tot);
		}
		for(y=65; y<129; ++y) { //DEP scr[0..64], scr4[1..65][65..128], PROD scr2[33..49][32..M-66]
			temp=0;
			for(k=-32; k<=32; ++k) //SDEP scr4[1..65][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
			temp=0;  
			for(k=-32; k<=32; ++k) //SDEP scr[y-65..y-1], SPROD scr2[33..49][y-33]
			{RECORD(SCR,1,0,;);
				RECORD(GAUSS,1,0, temp += scr[(y-33+k)%(SCRSIZE)] * Gauss[abs(k)]);}
			RECORD(SCR2,0,1, scr2[x%(SCR2X)][(y-33)%SCR2Y] =temp/tot);
		}
		for (y=16; y<80; ++y) //DEP scr2[0..49][0..M-1], PROD scr3[16..33][16..M-17]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[0..49][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[16..33][y]
		}
	}

	for (x=50; x<N-32; ++x){
		for(y=0;y<129;y++) {RECORD(SCR4,0,1, scr4[(x+32)%SCR4X][y%SCR4Y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[x+32][0..128]
		temp=0;
		for(y=32; y<=64; ++y){ //DEP scr4[x-32..x+32][32..64], PROD scr[32..64]
			for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
		}
		for(y=65; y<129; ++y) { //DEP scr[0..64], scr4[x-32..x+32][65..128], PROD scr[65..128], scr2[x][32..95]
			temp=0;
			for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] = temp/tot);
			temp=0;	  
			for(k=-32; k<=32; ++k) //SDEP scr[y-65..y-1], SPROD scr2[x][y-33]
			{RECORD(SCR,1,0,;);
				RECORD(GAUSS,1,0, temp += scr[(y-33+k)%(SCRSIZE)] * Gauss[abs(k)]);}
			RECORD(SCR2,0,1,scr2[x%(SCR2X)][(y-33)%SCR2Y] = temp/tot);
		}
		for (y=16; y<80; ++y) //DEP scr2[x-33..x-1][0..95], PROD scr3[x-17][16..79]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
		}
		for (y=16; y<64; ++y) { //DEP scr3[x-50..x-18][0..79], PROD image_outr[x-34][16..M-17]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
		}	
	}  

	for (x=N-32; x<N+1; ++x){
		for (y=16; y<80; ++y)
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff);
		}
		for (y=16; y<64; ++y) {
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp);
		}
	}

	for (x=N+1; x<N+18; ++x)
	for (y=16; y<64; ++y) {
		temp=255;
		for (x_offset=-16; x_offset <= 16; x_offset++)
		for (y_offset=-16; y_offset <= 16; y_offset++){
			RECORD(SCR3,2,0,;); //for line below
			if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
			temp=0;
		}
		RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp);
	}
	//END PROLOGUE
	
	int TS;
	for(TS = 64; TS+TH<M-64 ; TS += TH){ 
		printf("Tile %d...\n", (TS-64)/TH);
		
		//START OVERWRITE
		for (x=0; x<SCR2X; ++x) for (y=0; y<SCR2Y; ++y) RECORD(SCR2,0,1, scr2[x][y]=0); //0-init
		for (x=0; x<SCR3X; ++x) for (y=0; y<SCR3Y; ++y) RECORD(SCR3,0,1, scr3[x][y]=0);  //0-init	
		for (y=0; y<SCRSIZE; ++y) RECORD(SCR,0,1, scr[y]=0);
		
		for(x=0; x<=64; x++) {for(y=TS-64;y<=TS+TH+64;y++) {RECORD(SCR4,0,1, scr4[x%SCR4X][y%SCR4Y]=image_in[x][y]);RECORD(IN_IMAGE,1,0,;);}} //preload
		
		//lines
		//for(x=0; x<N; x++) image_outr[x][TS-1] = 255;
		//for(x=0; x<N; x++) image_outr[x][TS+TH+1] = 255;
		
		//MISSING: scr2[0..31][]
		//MISSING: scr3[0..15][]
		
		//x=32
		//HDEP scr4[0..64][TS-64..TS+TH+64]
		//HPROD scr2[32][TS-32..TS+TH+32]
		for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[0..64][TS-64..TS-1], PROD scr[TS-64..TS-1]
			temp=0;
			for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
		}	
		for(y=TS-32; y<=TS+TH+32; ++y) { //DEP scr4[0..64][TS..TS+TH+64], scr[TS-64..TS-1], PROD scr[TS..TS+TH+64], scr2[32][TS-32..TS+TH+32]
			temp=0;
			for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
			temp=0;	  
			for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
			{RECORD(SCR,1,0,;);
				RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
			RECORD(SCR2,0,1,scr2[32%(SCR2X)][(y)%SCR2Y] = temp/tot);
		}
		
		for(x=33; x<50; ++x){	
			//HDEP scr4[1..64][TS-64..TS+TH+64]
			//HDEP scr2[0..32][TS-32..TS+TH+32]
			//HPROD scr3[16..32][TS-16..TS+TH+16]
			//HPROD scr2[33..49][TS-32..TS+TH+32]
			//HPROD scr4[65..81][TS-64..TS+TH+64]
			for(y=TS-64;y<=TS+TH+64;y++) {RECORD(SCR4,0,1, scr4[(x+32)%SCR4X][y%SCR4Y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[x+32][TS-64..TS+TH+64]
			for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[x-32..x+32][TS-64..TS-1], PROD scr[TS-64..TS-1]
				temp=0;
				for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
				{RECORD(SCR4,1,0,;);
					RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
				RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
			}	
			for(y=TS-32; y<=TS+TH+32; ++y) { //DEP scr4[x-32..x+32][TS..TS+TH+64], scr[TS-64..TS-1], PROD scr[TS..TS+TH+64], scr2[x][TS-32..TS+TH+32]
				temp=0;
				for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
				{RECORD(SCR4,1,0,;);
					RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
				RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
				temp=0;	  
				for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
				{RECORD(SCR,1,0,;);
					RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
				RECORD(SCR2,0,1,scr2[x%(SCR2X)][(y)%SCR2Y] = image_outg[x][y] = temp/tot);
			}	
			for (y=TS-16; y<=TS+TH+16; ++y) //DEP scr2[x-33..x-1][TS-32..TS+TH+32], PROD scr3[x-17][TS-16..TS+TH+16]
			{
				maxdiff = 0;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
					RECORD(SCR2,2,0, ;); //for line below
					if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
					maxdiff = val;
				}
				RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
			}
		}
		
		for (x=50; x<N-32; ++x){ 
			//HDEP scr4[18..81][TS-64..TS+TH+64]
			//HDEP scr2[17..49][TS-32..TS+TH+32]
			//HDEP scr3[0..32][TS-16..TS+TH+16], 
			//HPROD scr4[82..N-1][TS-64..TS+TH+64]
			//HPROD scr2[50..N-33][TS-32..TS+TH+32]
			//HPROD scr3[33..N-50][TS-16..TS+TH+16]
			//HPROD image_outr[16..N-67][TS..TS+TH]
			for(y=TS-64;y<=TS+TH+64;y++) {RECORD(SCR4,0,1, scr4[(x+32)%SCR4X][y%SCR4Y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[x+32][TS-64..TS+TH+64]
			for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[x-32..x+32][TS-64..TS-1], PROD scr[TS-64..TS-1]
				temp=0;
				for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
				{RECORD(SCR4,1,0,;);
					RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
				RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
			}	
			for(y=TS-32; y<=TS+TH+32; ++y) { //DEP scr4[x-32..x+32][TS..TS+TH+64], scr[TS-64..TS-1], PROD scr[TS..TS+TH+64], scr2[x][TS-32..TS+TH+32]
				temp=0;
				for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
				{RECORD(SCR4,1,0,;);
					RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
				RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
				temp=0;	  
				for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
				{RECORD(SCR,1,0,;);
					RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
				RECORD(SCR2,0,1,scr2[x%(SCR2X)][(y)%SCR2Y] = image_outg[x][y] = temp/tot);
			}	
			for (y=TS-16; y<=TS+TH+16; ++y) //DEP scr2[x-33..x-1][TS-32..TS+TH+32], PROD scr3[x-17][TS-16..TS+TH+16]
			{
				maxdiff = 0;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
					RECORD(SCR2,2,0, ;); //for line below
					if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
					maxdiff = val;
				}
				RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
			}	
			for (y=TS; y<=TS+TH; ++y) { //DEP scr3[x-50..x-18][TS-16..TS+TH+16], PROD image_outr[x-34][TS..TS+TH]
				temp=255;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
					RECORD(SCR3,2,0,;); //for line below
					if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
					temp=0;
				}
				RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
			}		
		}
		
		//EPILOGUE
		for(x=N-32; x<N; x++){
			//HDEP scr2[N-65..N-33][TS-32..TS+TH+32]
			//HDEP scr3[N-82..N-50][TS-16..TS+TH+16]
			//HPROD scr3[N-49..N-18][TS-16..TS+TH+16]
			//HPROD image_outr[N-66..N-35][TS..TS+TH]
			//HPROD scr2[N-32..N-1]
			
			for(y=TS-32; y<=TS+TH+32; ++y) scr2[x%SCR2X][y%SCR2Y] = image_outg[x][y] = 0; //PROD scr2[x][TS-32..TS+TH+32]
			for (y=TS-16; y<=TS+TH+16; ++y) //DEP scr2[x-33..x-1][TS-32..TS+TH+32], PROD scr3[x-17][TS-16..TS+TH+16]
			{
				maxdiff = 0;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
					RECORD(SCR2,2,0, ;); //for line below
					if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
					maxdiff = val;
				}
				RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
			}			
			for (y=TS; y<=TS+TH; ++y) { //DEP scr3[x-50..x-18][TS-16..TS+TH+16], PROD image_outr[x-34][TS..TS+TH]
				temp=255;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
					RECORD(SCR3,2,0,;); //for line below
					if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
					temp=0;
				}
				RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
			}	
			
		}	
		//x=N
		//HDEP scr2[N-33..N-1][TS-32..TS+TH+32]
		//HDEP scr3[N-50..N-18][TS-16..TS+TH+16]
		//HPROD image_outr[N-34][TS..TS+TH]
		//HPROD scr3[N-17][TS-16..TS+TH+16]
		for (y=TS-16; y<=TS+TH+16; ++y) //DEP scr2[x-33..x-1][TS-32..TS+TH+32], PROD scr3[x-17][TS-16..TS+TH+16]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(N-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(N-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(N-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
		}			
		for (y=TS; y<=TS+TH; ++y) { //DEP scr3[x-50..x-18][TS-16..TS+TH+16], PROD image_outr[x-34][TS..TS+TH]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(N-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(N-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[N-34][y] = temp); //SPROD image_outr[x-34][y]
		}
		for(x=N+1; x<N+17; x++){
			//HDEP scr3[N-49..N-17][TS-16..TS+TH+16]
			//HPROD scr3[N-16..N-1][TS-16..TS+TH+16]
			//HPROD image_outr[N-33..N-18][TS..TS+TH]
			for (y=TS-16; y<=TS+TH+16; ++y) scr3[(x-17)%SCR3X][y%SCR3Y] = 0; //PROD scr3[x-17][TS-16..TS+TH+16]		
			for (y=TS; y<=TS+TH; ++y) { //DEP scr3[x-50..x-18][TS-16..TS+TH+16], PROD image_outr[x-34][TS..TS+TH]
				temp=255;
				for (x_offset=-16; x_offset <= 16; x_offset++)
				for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
					RECORD(SCR3,2,0,;); //for line below
					if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
					temp=0;
				}
				RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
			}	
			
		}
		//x=(N+17)
		//HDEP scr3[N-33..N-1][TS-16..TS+TH+16]
		//HPROD image_outr[N-17][TS..TS+TH]
		for (y=TS; y<=TS+TH; ++y) { //DEP scr3[x-50..x-18][TS-16..TS+TH+16], PROD image_outr[x-34][TS..TS+TH]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[((N+17)-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[((N+17)-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[(N+17)-34][y] = temp); //SPROD image_outr[x-34][y]
		}
		
	}
	
	/////////////////////////////////
	//EPILOGUE
	/////////////////////////////////
	
	//START OVERWRITE
	for (x=0; x<SCR2X; ++x) for (y=0; y<SCR2Y; ++y) RECORD(SCR2,0,1, scr2[x][y]=0); //0-init
	for (x=0; x<SCR3X; ++x) for (y=0; y<SCR3Y; ++y) RECORD(SCR3,0,1, scr3[x][y]=0);  //0-init	
	for (y=0; y<SCRSIZE; ++y) RECORD(SCR,0,1, scr[y]=0);
	
	for(x=0; x<=64; x++) {for(y=TS-64;y<M;y++) {RECORD(SCR4,0,1, scr4[x%SCR4X][y%SCR4Y]=image_in[x][y]);RECORD(IN_IMAGE,1,0,;);}} //preload
	
	//lines
	//for(x=0; x<N; x++) image_outr[x][TS-1] = 255;
	//for(x=0; x<N; x++) image_outr[x][TS+TH+1] = 255;
	
	//MISSING: scr2[0..31][]
	//MISSING: scr3[0..15][]
	
	//x=32
	//HDEP scr4[0..64][TS-64..TS+TH+64]
	//HPROD scr2[32][TS-32..TS+TH+32]
	for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[0..64][TS-64..TS-1], PROD scr[TS-64..TS-1]
		temp=0;
		for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
		{RECORD(SCR4,1,0,;);
			RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
		RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
	}	
	for(y=TS-32; y<=M-33; ++y) { //DEP scr4[0..64][TS..M-1], scr[TS-64..TS-1], PROD scr[TS..M-1], scr2[32][TS-32..M-33]
		temp=0;
		for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
		{RECORD(SCR4,1,0,;);
			RECORD(GAUSS,1,0, temp += scr4[(32+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
		RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
		temp=0;	  
		for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
		{RECORD(SCR,1,0,;);
			RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
		RECORD(SCR2,0,1,scr2[32%(SCR2X)][(y)%SCR2Y] = temp/tot);
	}
	
	for(x=33; x<50; ++x){
		for(y=TS-64;y<M;y++) {RECORD(SCR4,0,1, scr4[(x+32)%SCR4X][y%SCR4Y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[x+32][TS-64..M-1]
		for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[x-32..x+32][TS-64..TS-1], PROD scr[TS-64..TS-1]
			temp=0;
			for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
		}	
		for(y=TS-32; y<=M-33; ++y) { //DEP scr4[x-32..x+32][TS..M-1], scr[TS-64..TS-1], PROD scr[TS..M-1], scr2[x][TS-32..M-33]
			temp=0;
			for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
			temp=0;	  
			for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
			{RECORD(SCR,1,0,;);
				RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
			RECORD(SCR2,0,1,scr2[x%(SCR2X)][(y)%SCR2Y] = image_outg[x][y] = temp/tot);
		}	
		for (y=TS-16; y<=M-17; ++y) //DEP scr2[x-33..x-1][TS-32..M-1], PROD scr3[x-17][TS-16..M-17]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
		}
	}
	
	for (x=50; x<N-32; ++x){
		for(y=TS-64;y<M;y++) {RECORD(SCR4,0,1, scr4[(x+32)%SCR4X][y]=image_in[x+32][y]);RECORD(IN_IMAGE,1,0,;);} //PROD scr4[x+32][TS-64..M-1]
		for(y=TS-64; y<=TS-1; ++y){ //DEP scr4[x-32..x+32][TS-64..TS-1], PROD scr[TS-64..TS-1]
			temp=0;
			for (k=-32; k<=32; ++k) //SDEP scr4[x-32..x+32][y], SPROD scr[y]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][y%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[y%SCRSIZE] =temp/tot);
		}	
		for(y=TS-32; y<=M-33; ++y) { //DEP scr4[x-32..x+32][TS..M-1], scr[TS-64..TS-1], PROD scr[TS..M-1], scr2[x][TS-32..M-33]
			temp=0;
			for(k=-32; k<=32; ++k)  //SDEP scr4[x-32..x+32][y+32], SPROD scr[y+32]
			{RECORD(SCR4,1,0,;);
				RECORD(GAUSS,1,0, temp += scr4[(x+k)%SCR4X][(y+32)%SCR4Y] * Gauss[abs(k)]);}
			RECORD(SCR,0,1, scr[(y+32)%SCRSIZE] = temp/tot);
			temp=0;	  
			for(k=-32; k<=32; ++k) //SDEP scr[y-32..y+32], SPROD scr2[x][y]
			{RECORD(SCR,1,0,;);
				RECORD(GAUSS,1,0, temp += scr[(y+k)%(SCRSIZE)] * Gauss[abs(k)]);}
			RECORD(SCR2,0,1,scr2[x%(SCR2X)][(y)%SCR2Y] = image_outg[x][y] = temp/tot);
		}	
		for (y=TS-16; y<=M-17; ++y) //DEP scr2[x-33..x-1][TS-32..M-1], PROD scr3[x-17][TS-16..M-17]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
		}	
		for (y=TS; y<=M-17; ++y) { //DEP scr3[x-50..x-18][TS-16..M-1], PROD image_outr[x-34][TS..M-17]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
		}		
	}
	
	for(x=N-32; x<N; x++){		
		for(y=TS-64;y<M;y++) scr2[x%SCR2X][y%SCR2Y] = image_outg[x][y] = 0; //PROD scr4[x+32][TS-64..M-1]
		for (y=TS-16; y<=M-17; ++y) //DEP scr2[x-33..x-1][TS-32..M-1], PROD scr3[x-17][TS-16..M-17]
		{
			maxdiff = 0;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
				RECORD(SCR2,2,0, ;); //for line below
				if ( (val = abs(scr2[(x-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(x-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
				maxdiff = val;
			}
			RECORD(SCR3,0,1, scr3[(x-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
		}			
		for (y=TS; y<=M-17; ++y) { //DEP scr3[x-50..x-18][TS-16..M-1], PROD image_outr[x-34][TS..M-17]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
		}	
		
	}	
	//x=N
	for (y=TS-16; y<=M-17; ++y) //DEP scr2[x-33..x-1][TS-32..M-1], PROD scr3[x-17][TS-16..M-17]
	{
		maxdiff = 0;
		for (x_offset=-16; x_offset <= 16; x_offset++)
		for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr2[x-33..x-1][y-16..y+16]
			RECORD(SCR2,2,0, ;); //for line below
			if ( (val = abs(scr2[(N-17+x_offset)%(SCR2X)][(y+y_offset)%SCR2Y]-scr2[(N-17)%(SCR2X)][y%SCR2Y])) > maxdiff && !(x_offset == 0 && y_offset == 0))
			maxdiff = val;
		}
		RECORD(SCR3,0,1, scr3[(N-17)%SCR3X][y%SCR3Y] = maxdiff); //SPROD scr3[x-17][y]
	}			
	for (y=TS; y<=M-17; ++y) { //DEP scr3[x-50..x-18][TS-16..M-1], PROD image_outr[x-34][TS..M-17]
		temp=255;
		for (x_offset=-16; x_offset <= 16; x_offset++)
		for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
			RECORD(SCR3,2,0,;); //for line below
			if (scr3[(N-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(N-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
			temp=0;
		}
		RECORD(OUT_IMAGE,0,1, image_outr[N-34][y] = temp); //SPROD image_outr[x-34][y]
	}
	for(x=N+1; x<N+17; x++){
		for (y=M-16; y<M; ++y) scr3[(x-17)%SCR3X][y%SCR3Y] = 0; //PROD scr3[x-17][M-16..M-1]		
		for (y=TS; y<=M-17; ++y) { //DEP scr3[x-50..x-18][TS-16..M-1], PROD image_outr[x-34][TS..M-17]
			temp=255;
			for (x_offset=-16; x_offset <= 16; x_offset++)
			for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
				RECORD(SCR3,2,0,;); //for line below
				if (scr3[(x-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[(x-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
				temp=0;
			}
			RECORD(OUT_IMAGE,0,1, image_outr[x-34][y] = temp); //SPROD image_outr[x-34][y]
		}	
		
	}
	//x=(N+17)
	for (y=TS; y<=M-17; ++y) { //DEP scr3[x-50..x-18][TS-16..M-1], PROD image_outr[x-34][TS..M-17]
		temp=255;
		for (x_offset=-16; x_offset <= 16; x_offset++)
		for (y_offset=-16; y_offset <= 16; y_offset++){ //SDEP scr3[x-50..x-18][y-16..y+16]
			RECORD(SCR3,2,0,;); //for line below
			if (scr3[((N+17)-34+x_offset)%SCR3X][(y+y_offset)%SCR3Y] < scr3[((N+17)-34)%SCR3X][y%SCR3Y] && !(x_offset == 0 && y_offset == 0))
			temp=0;
		}
		RECORD(OUT_IMAGE,0,1, image_outr[(N+17)-34][y] = temp); //SPROD image_outr[x-34][y]
	}
}

int main(int argc, char* argv[])
{
	static int in_image[N][M];
	static int g_image[N][M];
	static int c_image[N][M];
	static int out_image[N][M];
	clock_t starttime, endtime;
	unsigned int i;

	for(i=0; i<BUFFERS_MAX; i++){
		reads[i] = 0;
		writes[i] = 0;
	}

	printf("  Reading image from disk (%s)...\n",IN_NAME);
	read_image(IN_NAME,in_image);

	printf("  Computing (32=%d)...\n",32);
	starttime=clock();
	GaussBlur(in_image, g_image, c_image, out_image);
	endtime=clock();

	printf("  Writing results to disk (%s)...\n",OUT_NAME);
	write_image(OUT_NAME1,g_image);
	//write_image(OUT_NAME2,c_image);
	write_image(OUT_NAME,out_image);

	printf("  Elapsed time (without disk reading & writing) is %f s\n",
	1.0*(endtime-starttime) / CLOCKS_PER_SEC);
	//getchar();


#ifdef DO_RECORD
	//calculate totals
	unsigned long long int tread, twrite;
	tread = twrite = 0;
	for(i=0; i<BUFFERS_MAX; i++){
		tread += reads[i];
		twrite += writes[i];
	}
	unsigned long long int taccess = tread + twrite;

	//calculate energies and times
	long long unsigned int writee[BUFFERS_MAX];
	long long unsigned int reade[BUFFERS_MAX];
	long long unsigned int cycles[BUFFERS_MAX];
	long long unsigned int totale = 0;
	long long unsigned int totalsz = 0;
	long long unsigned int totalcy = 0;
	for(i=0; i<BUFFERS_MAX; i++){
		writee[i] = ((writes[i]/1000) * BUFENERGIES[i])/1000000; //uJ
		reade[i] = ((reads[i]/1000) * BUFENERGIES[i])/1000000; //uJ
		cycles[i] = (reads[i]+writes[i])*BUFTIMES[i];
		totalcy += cycles[i];
		totale += reade[i] + writee[i];
		totalsz += BUFSIZES[i];
	}

	for(i=0; i<BUFFERS_MAX; i++){
		printf("%s: %llu R - %llu W - %llu T - %llu SZ - %llu CY - %llu uJ\n", BUFNAMES[i], reads[i], writes[i], reads[i] + writes[i], (long long unsigned int) BUFSIZES[i], cycles[i], reade[i] + writee[i]);
	}

	printf("TOTALS: %llu R - %llu W - %llu T - %llu SZ - %llu CY - %llu uJ\n", tread, twrite, taccess, totalsz, totalcy, totale);
#endif

}
