/* Initial version of cavity detector */

/* Not correct for boundaries */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define N 2144
#define M 2144

#define IN_NAME "test_im_med.pgm"
#define OUT_NAME "test.out.pgm"
#define OUT_NAME1 "test.out1.pgm"	// only used for output of temporary images
#define OUT_NAME2 "test.out2.pgm"	// idem

#define GB 32 
#define NB 16

#if GB <= 3
	int Gauss[]={99,68,35,10};
#elif GB == 4
	int Gauss[]={99,82,46,18,5};
#elif GB == 5
	int Gauss[]={99,88,61,33,14,5};
#elif GB == 6
	int Gauss[]={99,91,70,46,25,12,5};
#elif GB == 7
	int Gauss[]={99,93,77,56,36,21,10,5};
#elif GB == 8
	int Gauss[]={99,95,82,64,46,30,18,10,5};
#elif GB == 32
	int Gauss[]={99,99,98,97,95,92,89,86,82,78,73,69,64,60,55,50,46,41,37,33,30,26,23,20,18,15,13,11,10,8,7,6,5}; 
#elif GB==64
	int Gauss[]={99,99,99,99,98,98,97,96,95,94,92,91,89,88,86,84,82,80,78,76,73,71,69,67,64,62,60,57,55,53,50,48,46,44,41,39,37,35,33,32,30,28,26,25,23,22,20,19,18,16,15,14,13,12,11,10,10,9,8,7,7,6,6,5,5};
#else
	#error Dit mag niet!
#endif


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



void GaussBlur (int image_in[N][M], int image_out[N][M])
{
  static int tmp[N][M];
  int x,y,k;

  int tot=0;
  for (k=-GB; k<=GB; ++k) tot+=Gauss[abs(k)];

  for (x=0; x<N; ++x) for (y=0; y<M; ++y) tmp[x][y]=0;

  for (x=GB; x<=N-1-GB; ++x)
    for (y=GB; y<=M-1-GB; ++y) {
      for (k=-GB; k<=GB; ++k)
        tmp[x][y] += image_in[x+k][y] * Gauss[abs(k)];
      tmp[x][y] /= tot;
    }

  for (x=0; x<N; ++x) for (y=0; y<M; ++y) image_out[x][y]=0;

  for (x=GB; x<=N-1-GB; ++x)
    for (y=GB; y<=M-1-GB; ++y) {
      for (k=-GB; k<=GB; ++k)
        image_out[x][y] += tmp[x][y+k] * Gauss[abs(k)];
      image_out[x][y] /= tot;
    }
}


void ComputeEdges (int image_in[N][M], int image_out[N][M])
{
  int maxdiff,val;
  int x,y,x_offset,y_offset;

  for (x=0; x<N; ++x) for (y=0; y<M; ++y) image_out[x][y]=0;

  for (x=NB; x< N-NB; ++x)
    for (y=NB; y<M-NB; ++y)
    {
      maxdiff = 0;

      for (x_offset=-NB; x_offset <= NB; x_offset++)
	for (y_offset=-NB; y_offset <= NB; y_offset++)
      	   if ((val = abs(image_in[x+x_offset][y+y_offset]-image_in[x][y]))> maxdiff && !(x_offset == 0 && y_offset == 0))
       	  	maxdiff = val;

      image_out[x][y] = maxdiff;
    }
}


int maxval (int image[N][M])
{
  int max=0;
  int x,y;

  for (x=0; x<N; ++x)
    for (y=0; y<M; ++y)
      if (image[x][y]>max) max=image[x][y];
  return max;
}


void Reverse (int image_in[N][M], int image_out[N][M])
{
  int x,y;
  int max=maxval(image_in);

  for (x=0; x<N; ++x)
    for (y=0; y<M; ++y)
      image_out[x][y] = max - image_in[x][y];
}


void DetectRoots (int image_in[N][M], int image_out[N][M])
{
  int x,y,k,x_offset,y_offset;
  static int tmp[N][M];

  Reverse(image_in,tmp);

  for (x=0; x<N; ++x) for (y=0; y<M; ++y) image_out[x][y]=0;

  for (x=NB; x<=N-1-NB; ++x)
    for (y=NB; y<=M-1-NB; ++y) {
      image_out[x][y] = 255;
      for (x_offset=-NB; x_offset <= NB; x_offset++)
	for (y_offset=-NB; y_offset <= NB; y_offset++)
          if (tmp[x+x_offset][y+y_offset] > tmp[x][y] && !(x_offset == 0 && y_offset == 0))
            image_out[x][y] = 0;
    }
}



int main(int argc, char* argv[])
{
  static int in_image[N][M];
  static int g_image[N][M];
  static int c_image[N][M];
  static int out_image[N][M];
  clock_t starttime, endtime;

  printf("  Reading image from disk (%s)...\n",IN_NAME);
  read_image(IN_NAME,in_image);

  printf("  Computing (GB=%d)...\n",GB);
  starttime=clock();
    GaussBlur(in_image, g_image);
    ComputeEdges(g_image, c_image);
    DetectRoots(c_image, out_image);
  endtime=clock();

  printf("  Writing results to disk (%s)...\n",OUT_NAME);
//  write_image(OUT_NAME1,g_image);
//  write_image(OUT_NAME2,c_image);
  write_image(OUT_NAME,out_image);

  printf("  Elapsed time (without disk reading & writing) is %f s\n",
    1.0*(endtime-starttime) / CLOCKS_PER_SEC);
  //getchar();
}
