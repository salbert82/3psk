/*
	3psk - 3-pole Phase Shift Keying
	
	Copyright (c) Edward Cree, 2012
	Licensed under the GNU GPL v3+
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>

#include <atg.h>
#include <atg_internals.h>
#include "wav.h"
#include "bits.h"
#include "varicode.h"
#include "strbuf.h"

#define CONSLEN	1024
#define CONSDLEN	((int)floor(CONSLEN*min(bw, 750)/750))
#define BITBUFLEN	16
#define PHASLEN	25

#define INLINELEN	80
#define INLINES		5
#define OUTLINELEN	80
#define OUTLINES	5
#define MACROLEN	80
#define NMACROS		6

#define CONS_BG	(atg_colour){31, 31, 15, ATG_ALPHA_OPAQUE}
#define PHAS_BG	(atg_colour){15, 31, 31, ATG_ALPHA_OPAQUE}
#define SPEC_BG	(atg_colour){ 7,  7, 23, ATG_ALPHA_OPAQUE}

const char *set_tbl[6]={
"Baud Rates",
"BW min max",
"10   1   7",
"30   1  20",
"150  4  90",
"750 40 400"};

int pset(SDL_Surface *s, unsigned int x, unsigned int y, atg_colour c);
int line(SDL_Surface *s, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, atg_colour c);
void ztoxy(fftw_complex z, double gsf, int *x, int *y);
atg_element *create_selector(unsigned int *sel);

int main(int argc, char **argv)
{
	double centre=440; // centre frequency, Hz
	double aif=3000; // approximate IF, Hz
	unsigned int slow=28;
	unsigned int am=10;
	bool moni=true, afc=false;
	unsigned int txbaud=60;
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--freq=", 7)==0)
		{
			sscanf(argv[arg]+7, "%lg", &centre);
		}
		else if(strncmp(argv[arg], "--slow=", 7)==0)
		{
			sscanf(argv[arg]+7, "%u", &slow);
		}
		else if(strncmp(argv[arg], "--if=", 5)==0)
		{
			sscanf(argv[arg]+5, "%lg", &aif);
		}
		else if(strncmp(argv[arg], "--am=", 5)==0)
		{
			sscanf(argv[arg]+5, "%u", &am);
		}
		else if(strncmp(argv[arg], "--txb=", 6)==0)
		{
			sscanf(argv[arg]+6, "%u", &txbaud);
		}
		else if(strcmp(argv[arg], "--moni")==0)
		{
			moni=true;
		}
		else if(strcmp(argv[arg], "--no-moni")==0)
		{
			moni=false;
		}
		else if(strcmp(argv[arg], "--afc")==0)
		{
			afc=true;
		}
		else if(strcmp(argv[arg], "--no-afc")==0)
		{
			afc=false;
		}
		else
		{
			fprintf(stderr, "Unrecognised arg `%s'\n", argv[arg]);
			return(1);
		}
	}
	double txcentre=centre;
	fprintf(stderr, "Constructing GUI\n");
	atg_canvas *canvas=atg_create_canvas(480, 320, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
	if(!canvas)
	{
		fprintf(stderr, "atg_create_canvas failed\n");
		return(1);
	}
	SDL_WM_SetCaption("3PSK", "3PSK");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	atg_box *mainbox=canvas->box;
	atg_element *g_title=atg_create_element_label("3psk by Edward Cree M0TBK", 12, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
	if(!g_title)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	g_title->cache=true;
	if(atg_pack_element(mainbox, g_title))
	{
		perror("atg_pack_element");
		return(1);
	}
	atg_element *g_decoder=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
	if(!g_decoder)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_pack_element(mainbox, g_decoder))
	{
		perror("atg_pack_element");
		return(1);
	}
	char *g_bauds=NULL;
	bool *g_tx=NULL;
	unsigned int bwsel=2;
	atg_element *g_bw=NULL, *g_spl=NULL, *g_txf=NULL, *g_rxf=NULL;
	SDL_Surface *g_constel_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 120, 120, canvas->surface->format->BitsPerPixel, canvas->surface->format->Rmask, canvas->surface->format->Gmask, canvas->surface->format->Bmask, canvas->surface->format->Amask);
	if(!g_constel_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	SDL_Surface *g_phasing_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 100, 120, canvas->surface->format->BitsPerPixel, canvas->surface->format->Rmask, canvas->surface->format->Gmask, canvas->surface->format->Bmask, canvas->surface->format->Amask);
	if(!g_phasing_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	SDL_Surface *g_spectro_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 160, 60, canvas->surface->format->BitsPerPixel, canvas->surface->format->Rmask, canvas->surface->format->Gmask, canvas->surface->format->Bmask, canvas->surface->format->Amask);
	if(!g_spectro_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	else
	{
		SDL_FillRect(g_constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(g_constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
		SDL_FillRect(g_phasing_img, &(SDL_Rect){0, 0, 100, 120}, SDL_MapRGB(g_phasing_img->format, PHAS_BG.r, PHAS_BG.g, PHAS_BG.b));
		SDL_FillRect(g_spectro_img, &(SDL_Rect){0, 0, 160,  60}, SDL_MapRGB(g_spectro_img->format, SPEC_BG.r, SPEC_BG.g, SPEC_BG.b));
		atg_box *g_db=g_decoder->elem.box;
		if(!g_db)
		{
			fprintf(stderr, "g_decoder->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_constel=atg_create_element_image(g_constel_img);
		if(!g_constel)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_constel))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_phasing=atg_create_element_image(g_phasing_img);
		if(!g_phasing)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_phasing))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_controls=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE});
		if(!g_controls)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_controls))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_box *b=g_controls->elem.box;
		if(!b)
		{
			fprintf(stderr, "g_controls->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_btns1=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 23, 23, ATG_ALPHA_OPAQUE});
		if(!g_btns1)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(b, g_btns1))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_box *bb1=g_btns1->elem.box;
		if(!bb1)
		{
			fprintf(stderr, "g_btns1->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_tx_t=atg_create_element_toggle("TX", false, (atg_colour){191, 0, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_tx_t)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		atg_toggle *t=g_tx_t->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_tx_t->elem.toggle==NULL\n");
			return(1);
		}
		g_tx=&t->state;
		g_tx_t->userdata="TX";
		if(atg_pack_element(bb1, g_tx_t))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_moni=atg_create_element_toggle("MONI", moni, (atg_colour){0, 191, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_moni)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		g_moni->userdata="MONI";
		if(atg_pack_element(bb1, g_moni))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_afc=atg_create_element_toggle("AFC", afc, (atg_colour){191, 127, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_afc)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		g_afc->userdata="AFC";
		if(atg_pack_element(bb1, g_afc))
		{
			perror("atg_pack_element");
			return(1);
		}
		g_spl=atg_create_element_toggle("SPL", false, (atg_colour){191, 191, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_spl)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		g_spl->userdata="SPL";
		if(atg_pack_element(bb1, g_spl))
		{
			perror("atg_pack_element");
			return(1);
		}
		g_bw=create_selector(&bwsel);
		if(!g_afc)
		{
			fprintf(stderr, "create_selector failed\n");
			return(1);
		}
		if(atg_pack_element(b, g_bw))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_txbaud=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 600, 1, txbaud, "TXB %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_txbaud)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_txbaud->userdata="TXBAUD";
		if(atg_pack_element(b, g_txbaud))
		{
			perror("atg_pack_element");
			return(1);
		}
		g_txf=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 200, 800, 1, txcentre, "TXF %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_txf)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_txf->userdata="TXFREQ";
		if(atg_pack_element(b, g_txf))
		{
			perror("atg_pack_element");
			return(1);
		}
		g_rxf=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 200, 800, 1, centre, "RXF %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_rxf)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_rxf->userdata="RXFREQ";
		if(atg_pack_element(b, g_rxf))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_slow=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 1, 64, 1, slow, "RXS %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_slow)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_slow->userdata="SLOW";
		if(atg_pack_element(b, g_slow))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_amp=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 25, 1, am, "AMP %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!g_amp)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		g_amp->userdata="AMP";
		if(atg_pack_element(b, g_amp))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_baud_label=atg_create_element_label("RXB 000", 15, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
		if(!g_baud_label)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!g_baud_label->elem.label)
		{
			fprintf(stderr, "g_baud_label->elem.label==NULL\n");
			return(1);
		}
		g_bauds=g_baud_label->elem.label->text;
		if(!g_bauds)
		{
			fprintf(stderr, "g_bauds==NULL\n");
			return(1);
		}
		if(atg_pack_element(b, g_baud_label))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_rbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_rbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(g_db, g_rbox))
		{
			perror("atg_pack_element");
			return(1);
		}
		b=g_rbox->elem.box;
		atg_element *g_spectro=atg_create_element_image(g_spectro_img);
		if(!g_spectro)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		g_spectro->clickable=true;
		g_spectro->userdata="SPEC";
		if(atg_pack_element(b, g_spectro))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_set_tbl=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){31, 31, 31, ATG_ALPHA_OPAQUE});
		if(!g_set_tbl)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		g_set_tbl->cache=true;
		if(atg_pack_element(b, g_set_tbl))
		{
			perror("atg_pack_element");
			return(1);
		}
		b=g_set_tbl->elem.box;
		if(!b)
		{
			fprintf(stderr, "g_set_tbl->elem.box==NULL\n");
			return(1);
		}
		for(size_t i=0;i<6;i++)
		{
			atg_element *l=atg_create_element_label(set_tbl[i], 12, (atg_colour){239, 239, 255, ATG_ALPHA_OPAQUE});
			if(!l)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			if(atg_pack_element(b, l))
			{
				perror("atg_pack_element");
				return(1);
			}
		}
	}
	char *outtext[OUTLINES];
	for(unsigned int i=0;i<OUTLINES;i++)
	{
		atg_element *line=atg_create_element_label(NULL, 8, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
		if(!line)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(!line->elem.label)
		{
			fprintf(stderr, "line->elem.label==NULL\n");
			return(1);
		}
		if(!(outtext[i]=line->elem.label->text=malloc(OUTLINELEN+2)))
		{
			perror("malloc");
			return(1);
		}
		outtext[i][0]=' ';
		outtext[i][1]=0;
		if(atg_pack_element(mainbox, line))
		{
			perror("atg_pack_element");
			return(1);
		}
	}
	char *intextleft[INLINES], *intextright[INLINES];
	char *inr, *ing; size_t inrl, inri, ingl, ingi;
	init_char(&inr, &inrl, &inri);
	init_char(&ing, &ingl, &ingi);
	for(unsigned int i=0;i<INLINES;i++)
	{
		atg_element *in_line=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){7, 7, 31, ATG_ALPHA_OPAQUE});
		if(!in_line)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_pack_element(mainbox, in_line))
		{
			perror("atg_pack_element");
			return(1);
		}
		else
		{
			atg_box *b=in_line->elem.box;
			if(!b)
			{
				fprintf(stderr, "in_line->elem.box==NULL\n");
				return(1);
			}
			atg_element *lsp=atg_create_element_label(" ", 8, (atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE});
			if(!lsp)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			if(atg_pack_element(b, lsp))
			{
				perror("atg_pack_element");
				return(1);
			}
			atg_element *left=atg_create_element_label(NULL, 8, (atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE});
			if(!left)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			if(!left->elem.label)
			{
				fprintf(stderr, "left->elem.label==NULL\n");
				return(1);
			}
			if(!(intextleft[i]=left->elem.label->text=malloc(INLINELEN+1)))
			{
				perror("malloc");
				return(1);
			}
			if(atg_pack_element(b, left))
			{
				perror("atg_pack_element");
				return(1);
			}
			atg_element *right=atg_create_element_label(NULL, 8, (atg_colour){255, 127, 0, ATG_ALPHA_OPAQUE});
			if(!right)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			if(!right->elem.label)
			{
				fprintf(stderr, "right->elem.label==NULL\n");
				return(1);
			}
			if(!(intextright[i]=right->elem.label->text=malloc(INLINELEN+1)))
			{
				perror("malloc");
				return(1);
			}
			if(atg_pack_element(b, right))
			{
				perror("atg_pack_element");
				return(1);
			}
		}
	}
	atg_colour *mcol[NMACROS];
	char *macro[NMACROS];
	atg_element *mline[NMACROS];
	for(unsigned int i=0;i<NMACROS;i++)
	{
		mline[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE});
		if(!mline[i])
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		mline[i]->w=canvas->surface->w;
		mline[i]->clickable=true;
		if(atg_pack_element(mainbox, mline[i]))
		{
			perror("atg_pack_element");
			return(1);
		}
		else
		{
			atg_box *b=mline[i]->elem.box;
			if(!b)
			{
				fprintf(stderr, "mline[%u]->elem.box==NULL\n", i);
				return(1);
			}
			mcol[i]=&b->bgcolour;
			atg_element *fn=atg_create_element_label("Fn: ", 8, (atg_colour){255, 255, 0, ATG_ALPHA_OPAQUE});
			if(!fn)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			else
			{
				atg_label *l=fn->elem.label;
				if(!l)
				{
					fprintf(stderr, "fn->elem.label==NULL\n");
					return(1);
				}
				if(!l->text)
				{
					fprintf(stderr, "l->text==NULL\n");
					return(1);
				}
				snprintf(l->text, 5, "F%01u: ", i+1);
			}
			if(atg_pack_element(b, fn))
			{
				perror("atg_pack_element");
				return(1);
			}
			atg_element *text=atg_create_element_label(NULL, 8, (atg_colour){255, 255, 0, ATG_ALPHA_OPAQUE});
			if(!text)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			if(!text->elem.label)
			{
				fprintf(stderr, "left->elem.label==NULL\n");
				return(1);
			}
			if(!(macro[i]=text->elem.label->text=malloc(MACROLEN+1)))
			{
				perror("malloc");
				return(1);
			}
			macro[i][0]=0;
			if(atg_pack_element(b, text))
			{
				perror("atg_pack_element");
				return(1);
			}
		}
	}
	unsigned int inp=NMACROS;
	
	fprintf(stderr, "Setting up decoder frontend\n");
	wavhdr w;
	if(read_wh44(stdin, &w))
	{
		fprintf(stderr, "Failed to read WAV header; werrno=%d\n", werrno);
		return(1);
	}
	if(w.num_channels!=1) // TODO handle a stereo I/Q (ie. complex) input
	{
		fprintf(stderr, "Too many channels; input WAV stream should be mono\n");
		return(1);
	}
	
	fprintf(stderr, "frontend: Audio sample rate: %lu Hz\n", (unsigned long)w.sample_rate);
	unsigned long blklen=floor(w.sample_rate/150.0);
	double bw=w.sample_rate/(double)blklen;
	
	fprintf(stderr, "frontend: FFT block length: %lu samples\n", blklen);
	if(blklen>4095)
	{
		fprintf(stderr, "frontend: FFT block length too long.  Exiting\n");
		return(1);
	}
	fprintf(stderr, "frontend: Filter bandwidth is %g Hz\n", bw);
	unsigned long k = max(floor((aif * (double)blklen / (double)w.sample_rate)+0.5), 1);
	double truif=(k*w.sample_rate/(double)blklen);
	fprintf(stderr, "frontend: Actual IF: %g Hz\n", truif);
	fftw_complex *fftin=fftw_malloc(sizeof(fftw_complex)*floor(w.sample_rate/10.0));
	fftw_complex *fftout=fftw_malloc(sizeof(fftw_complex)*floor(w.sample_rate/10.0));
	unsigned long speclen=max(floor(w.sample_rate/5.0), 360), spechalf=(speclen>>1)+1;
	double spec_hpp=w.sample_rate/(double)speclen;
	double *specin=fftw_malloc(sizeof(double)*speclen);
	fftw_complex *specout=fftw_malloc(sizeof(fftw_complex)*spechalf);
	fftw_set_timelimit(2.0);
	fprintf(stderr, "frontend: Preparing FFT plans\n");
	fftw_plan p[4], sp_p;
	p[0]=fftw_plan_dft_1d(floor(w.sample_rate/10.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[1]=fftw_plan_dft_1d(floor(w.sample_rate/30.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[2]=fftw_plan_dft_1d(floor(w.sample_rate/150.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	p[3]=fftw_plan_dft_1d(floor(w.sample_rate/750.0), fftin, fftout, FFTW_FORWARD, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	sp_p=fftw_plan_dft_r2c_1d(speclen, specin, specout, FFTW_DESTROY_INPUT|FFTW_PATIENT);
	fprintf(stderr, "frontend: Ready\n");
	long wzero=zeroval(w);
	
	fprintf(stderr, "Setting up decoder\n");
	double gsf=250.0/(double)blklen;
	double sens=(double)blklen/16.0;
	
	fftw_complex points[CONSLEN];
	bool lined[CONSLEN];
	for(size_t i=0;i<CONSLEN;i++)
	{
		points[i]=0;
		lined[i]=false;
	}
	fftw_complex lastsym=0;
	
	bool bitbuf[BITBUFLEN];
	unsigned int bitbufp=0;
	
	unsigned int da_ptr=0;
	double old_da[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		old_da[i]=0;
	double t_da=0;
	bool fch=false;
	unsigned int st_ptr=0;
	unsigned int symtime[PHASLEN];
	for(size_t i=0;i<PHASLEN;i++)
		symtime[i]=0;
	bool st_loop=false;
	
	unsigned int spec_pt[8][160];
	for(unsigned int i=0;i<8;i++)
		for(unsigned int j=0;j<160;j++)
			spec_pt[i][j]=60;
	unsigned int spec_which=0;
	
	bool transmit=false;
	double txphi=0;
	bbuf txbits={0, NULL};
	unsigned int txsetp=0;
	unsigned int txlead=0;
	
	if(write_wh44(stdout, w))
	{
		fprintf(stderr, "Failed to write WAV header\n");
		return(1);
	}
	int errupt=0;
	fprintf(stderr, "Starting main loop\n");
	for(unsigned int t=0;((t<w.num_samples)||(w.data_len==(uint32_t)(-1)))&&!errupt;t++)
	{
		if(!(t%blklen))
		{
			static unsigned int frame=0, lastflip=0;
			fftw_execute(p[bwsel]);
			int x,y;
			ztoxy(points[frame%CONSDLEN], gsf, &x, &y);
			if(lined[frame%CONSDLEN]) line(g_constel_img, x, y, 60, 60, CONS_BG);
			pset(g_constel_img, x, y, CONS_BG);
			fftw_complex half=points[(frame+(CONSDLEN>>1))%CONSDLEN];
			ztoxy(half, gsf, &x, &y);
			atg_colour c=(cabs(half)>sens)?(atg_colour){0, 127, 0, ATG_ALPHA_OPAQUE}:(atg_colour){127, 0, 0, ATG_ALPHA_OPAQUE};
			if(lined[(frame+(CONSDLEN>>1))%CONSDLEN]) line(g_constel_img, x, y, 60, 60, (atg_colour){0, 95, 95, ATG_ALPHA_OPAQUE});
			pset(g_constel_img, x, y, c);
			ztoxy(points[frame%CONSDLEN]=fftout[k], gsf, &x, &y);
			bool green=cabs(fftout[k])>sens;
			bool enough=false;
			double da=0;
			if(cabs(lastsym)>sens)
				enough=(fabs(da=carg(fftout[k]/lastsym))>(fch?M_PI*2/3.0:M_PI/2));
			else
				enough=true;
			fftw_complex dz=bwsel?fftout[k]-points[(frame+CONSDLEN-1)%CONSDLEN]:fftout[k]/points[(frame+CONSDLEN-1)%CONSDLEN];
			bool spd=bwsel?(cabs(dz)<cabs(fftout[k])*blklen*blklen/(exp2(((signed)slow-32)/4.0)*2e4)):(fabs(carg(dz))<blklen/(exp2(((signed)slow-32)/4.0)*2e2));
			if((lined[frame%CONSDLEN]=(green&&enough&&(fch||spd))))
			{
				line(g_constel_img, x, y, 60, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				line(g_phasing_img, 0, 60, g_phasing_img->w, 60, (atg_colour){0, 191, 191, ATG_ALPHA_OPAQUE});
				int py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(g_phasing_img, da_ptr*g_phasing_img->w/PHASLEN, py, (da_ptr+1)*g_phasing_img->w/PHASLEN, py, PHAS_BG);
				old_da[da_ptr]=(da<0)?da+M_PI*4/3.0:da;
				t_da+=old_da[da_ptr]-M_PI*2/3.0;
				py=60+(old_da[da_ptr]-M_PI*2/3.0)*60;
				line(g_phasing_img, da_ptr*g_phasing_img->w/PHASLEN, py, (da_ptr+1)*g_phasing_img->w/PHASLEN, py, (da>0)?(atg_colour){255, 191, 255, ATG_ALPHA_OPAQUE}:(atg_colour){255, 255, 127, ATG_ALPHA_OPAQUE});
				unsigned int dt=t-symtime[st_ptr];
				double baud=w.sample_rate*(st_loop?PHASLEN:st_ptr)/(double)dt;
				snprintf(g_bauds, 8, "RXB %03d", (int)floor(baud+.5));
				symtime[st_ptr]=t;
				st_ptr=(st_ptr+1)%PHASLEN;
				if(!st_ptr) st_loop=true;
				da_ptr=(da_ptr+1)%PHASLEN;
				if(afc&&!da_ptr)
				{
					double ch=(t_da/(double)PHASLEN)*10.0;
					if(fabs(ch)>0.5)
					{
						centre+=ch;
						if(g_rxf&&(g_rxf->type==ATG_SPINNER))
						{
							atg_spinner *s=g_rxf->elem.spinner;
							if(s) s->value=floor(centre+.5);
						}
						fch=true;
					}
					t_da=0;
				}
				lastsym=fftout[k];
				if(bitbufp>=BITBUFLEN)
				{
					bitbufp--;
					for(unsigned int i=0;i<bitbufp;i++)
						bitbuf[i]=bitbuf[i+1];
				}
				bitbuf[bitbufp++]=(da>0);
				int ubits=0;
				char *text=decode((bbuf){.nbits=bitbufp, .data=bitbuf}, &ubits);
				if(*text)
				{
					size_t tp=strlen(outtext[OUTLINES-1]);
					for(const char *p=text;*p;p++)
					{
						if((*p=='\n')||(tp>OUTLINELEN))
						{
							for(unsigned int i=0;i<OUTLINES-1;i++)
								strcpy(outtext[i], outtext[i+1]);
							outtext[OUTLINES-1][0]=' ';
							outtext[OUTLINES-1][1]=0;
							tp=1;
						}
						if(*p=='\t')
						{
							outtext[OUTLINES-1][tp++]=' ';
							while((tp<OUTLINELEN)&&((tp-1)&3))
								outtext[OUTLINES-1][tp++]=' ';
							outtext[OUTLINES-1][tp]=0;
						}
						else if(*p!='\n')
						{
							outtext[OUTLINES-1][tp++]=*p;
							outtext[OUTLINES-1][tp]=0;
						}
					}
				}
				bitbufp-=ubits;
				for(unsigned int i=0;i<bitbufp;i++)
					bitbuf[i]=bitbuf[i+ubits];
				free(text);
			}
			fch=false;
			pset(g_constel_img, x, y, green?(atg_colour){0, 255, 0, ATG_ALPHA_OPAQUE}:(atg_colour){255, 0, 0, ATG_ALPHA_OPAQUE});
			frame++;
			if(t>(lastflip+w.sample_rate/8))
			{
				lastflip+=w.sample_rate/8;
				if(g_tx) *g_tx=transmit;
				if(g_spl&&(g_spl->type==ATG_TOGGLE)&&g_spl->elem.toggle)
					g_spl->elem.toggle->state=(centre!=txcentre);
				if(true)
				{
					if(ingi>((INLINES+1)*INLINELEN))
					{
						ingi-=INLINELEN;
						memmove(ing, ing+INLINELEN, ingi);
					}
					for(unsigned int i=0;i<INLINES;i++)
						intextleft[i][0]=intextright[i][0]=0;
					unsigned int x=0,y=0;
					for(size_t p=0;p<ingi;p++)
					{
						intextleft[y][x++]=ing[p];
						intextleft[y][x]=0;
						if((ing[p]=='\n')||(x>=INLINELEN))
						{
							if(y<INLINES-1)
								y++;
							else
							{
								for(unsigned int i=0;i<y;i++)
									strcpy(intextleft[i], intextleft[i+1]);
								intextleft[y][0]=0;
							}
							x=0;
						}
					}
					unsigned int sx=0;
					for(size_t p=0;p<inri;p++)
					{
						x++;
						intextright[y][sx++]=inr[p];
						intextright[y][sx]=0;
						if((inr[p]=='\n')||(x>=INLINELEN))
						{
							if(y<INLINES-1)
								y++;
							else
							{
								for(unsigned int i=0;i<y;i++)
								{
									strcpy(intextleft[i], intextleft[i+1]);
									strcpy(intextright[i], intextright[i+1]);
								}
								intextleft[y][0]=intextright[y][0]=0;
							}
							x=sx=0;
						}
					}
				}
				for(unsigned int i=0;i<NMACROS;i++)
					*mcol[i]=(i==inp)?(atg_colour){63, 63, 47, ATG_ALPHA_OPAQUE}:(atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE};
				atg_flip(canvas);
				atg_event e;
				while(atg_poll_event(&e, canvas))
				{
					switch(e.type)
					{
						case ATG_EV_RAW:;
							SDL_Event s=e.event.raw;
							switch(s.type)
							{
								case SDL_QUIT:
									errupt=1;
								break;
								case SDL_KEYDOWN:
									switch(s.key.keysym.sym)
									{
										case SDLK_F1:
											append_str(&inr, &inrl, &inri, macro[0]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F2:
											append_str(&inr, &inrl, &inri, macro[1]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F3:
											append_str(&inr, &inrl, &inri, macro[2]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F4:
											append_str(&inr, &inrl, &inri, macro[3]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F5:
											append_str(&inr, &inrl, &inri, macro[4]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F6:
											append_str(&inr, &inrl, &inri, macro[5]);
											if(!transmit)
											{
												transmit=true;
												txlead=max(txbaud, 8);
											}
										break;
										case SDLK_F7:
											transmit=true;
											txlead=max(txbaud, 8);
										break;
										case SDLK_F8:
											transmit=false;
											txlead=max(txbaud/2, 8);
										break;
										case SDLK_ESCAPE:
											transmit=false;
											txlead=0;
											inri=0;
										break;
										case SDLK_F9:
											centre=txcentre;
											if(g_rxf&&(g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=g_rxf->elem.spinner;
												if(s) s->value=floor(centre+.5);
											}
										break;
										case SDLK_BACKSPACE:
										{
											if(inp>=NMACROS)
											{
												if(inri) inr[--inri]=0;
											}
											else
											{
												size_t l=strlen(macro[inp]);
												if(l) macro[inp][l-1]=0;
											}
										}
										break;
										case SDLK_RETURN:
											if(inp>=NMACROS)
												append_char(&inr, &inrl, &inri, '\n');
											else
											{
												size_t l=strlen(macro[inp]);
												if(l<MACROLEN-1)
												{
													macro[inp][l++]='\n';
													macro[inp][l]=0;
												}
											}
										break;
										case SDLK_KP_ENTER:
											if(inp>=NMACROS)
												append_char(&inr, &inrl, &inri, '\r');
											else
											{
												size_t l=strlen(macro[inp]);
												if(l<MACROLEN-1)
												{
													macro[inp][l++]='\r';
													macro[inp][l]=0;
												}
											}
										break;
										default:
											if((s.key.keysym.unicode&0xFF80)==0)
											{
												char what=s.key.keysym.unicode&0x7F;
												if(what)
												{
													if(inp>=NMACROS)
														append_char(&inr, &inrl, &inri, what);
													else
													{
														size_t l=strlen(macro[inp]);
														if(l<MACROLEN-1)
														{
															macro[inp][l++]=what;
															macro[inp][l]=0;
														}
													}
												}
											}
										break;
									}
								break;
							}
						break;
						case ATG_EV_CLICK:;
							atg_ev_click click=e.event.click;
							if(!click.e)
								fprintf(stderr, "click.e==NULL\n");
							else if(click.e->userdata)
							{
								if(strcmp(click.e->userdata, "SPEC")==0)
								{
									switch(click.button)
									{
										case ATG_MB_LEFT:
											txcentre=(click.pos.x+20)*spec_hpp;
											if(g_txf&&(g_txf->type==ATG_SPINNER))
											{
												atg_spinner *s=g_txf->elem.spinner;
												if(s) s->value=floor(txcentre+.5);
											}
											/* fallthrough */
										case ATG_MB_RIGHT:
											centre=(click.pos.x+20)*spec_hpp;
											if(g_rxf&&(g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=g_rxf->elem.spinner;
												if(s) s->value=floor(centre+.5);
											}
										break;
										default:
											// ignore
										break;
									}
								}
							}
							else
							{
								for(unsigned int i=0;i<NMACROS;i++)
									if(click.e==mline[i]) inp=(inp==i)?NMACROS:i;
							}
						break;
						case ATG_EV_TRIGGER:;
							atg_ev_trigger trigger=e.event.trigger;
							if(!trigger.e)
							{
								fprintf(stderr, "trigger.e==NULL\n");
							}
							else
								fprintf(stderr, "Clicked on unknown button!\n");
						break;
						case ATG_EV_VALUE:;
							atg_ev_value value=e.event.value;
							if(value.e)
							{
								if(value.e==g_bw)
								{
									bw=(double[4]){10, 30, 150, 750}[value.value];
									blklen=floor(w.sample_rate/bw);
									bw=w.sample_rate/(double)blklen;
									fprintf(stderr, "frontend: new FFT block length: %lu samples\n", blklen);
									if(blklen>4095)
									{
										fprintf(stderr, "frontend: new FFT block length too long.  Exiting\n");
										return(1);
									}
									fprintf(stderr, "frontend: new filter bandwidth is %g Hz\n", bw);
									k=max(floor((aif * (double)blklen / (double)w.sample_rate)+0.5), 1);
									truif=(k*w.sample_rate/(double)blklen);
									fprintf(stderr, "frontend: new Actual IF: %g Hz\n", truif);
									gsf=250.0/(double)blklen;
									sens=(double)blklen/16.0;
									SDL_FillRect(g_constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(g_constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
									for(size_t i=0;i<CONSLEN;i++)
									{
										points[i]=0;
										lined[i]=false;
									}
									fprintf(stderr, "frontend: all ready to go with new bandwidth\n");
								}
								else if(value.e->userdata)
								{
									if(strcmp((const char *)value.e->userdata, "SLOW")==0)
									{
										slow=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "TXBAUD")==0)
									{
										txbaud=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "TXFREQ")==0)
									{
										bool spl=(centre!=txcentre);
										txcentre=value.value;
										if(!spl)
										{
											centre=txcentre;
											if(g_rxf&&(g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=g_rxf->elem.spinner;
												if(s) s->value=floor(centre+.5);
											}
										}
									}
									else if(strcmp((const char *)value.e->userdata, "RXFREQ")==0)
									{
										centre=value.value;
									}
									else if(strcmp((const char *)value.e->userdata, "AMP")==0)
									{
										am=value.value;
									}
									else
										fprintf(stderr, "Changed an unknown spinner!\n");
								}
							}
						break;
						case ATG_EV_TOGGLE:;
							atg_ev_toggle toggle=e.event.toggle;
							if(toggle.e)
							{
								if(toggle.e->userdata)
								{
									if(strcmp((const char *)toggle.e->userdata, "TX")==0)
									{
										transmit=toggle.state;
										txlead=max(txbaud/(transmit?1:2), 8);
									}
									else if(strcmp((const char *)toggle.e->userdata, "MONI")==0)
									{
										moni=toggle.state;
									}
									else if(strcmp((const char *)toggle.e->userdata, "AFC")==0)
									{
										afc=toggle.state;
									}
									else if(strcmp((const char *)toggle.e->userdata, "SPL")==0)
									{
										if(!toggle.state)
										{
											centre=txcentre;
											if(g_rxf&&(g_rxf->type==ATG_SPINNER))
											{
												atg_spinner *s=g_rxf->elem.spinner;
												if(s) s->value=floor(centre+.5);
											}
										}
									}
									else
										fprintf(stderr, "Clicked on unknown toggle '%s'!\n", (const char *)toggle.e->userdata);
								}
							}
						break;
						default:
						break;
					}
				}
			}
		}
		long si=read_sample(w, stdin)-wzero;
		if(transmit||txlead)
		{
			if(((t*txbaud)%w.sample_rate)<txbaud)
			{
				if(txlead)
				{
					txlead--;
					txsetp=(txsetp+2)%3;
				}
				else
				{
					if(!txbits.data) txbits.nbits=0;
					if(inr&&inri&&!txbits.nbits)
					{
						free(txbits.data);
						const char buf[2]={inr[0], 0};
						if(*buf=='\r')
						{
							transmit=false;
							txlead=max(txbaud/2, 8);
							txbits=(bbuf){0, NULL};
						}
						else
							txbits=encode(buf);
						append_char(&ing, &ingl, &ingi, *buf);
						inri--;
						for(size_t i=0;i<inri;i++)
							inr[i]=inr[i+1];
					}
					if(txbits.nbits)
					{
						bool b=*txbits.data;
						txbits.nbits--;
						for(size_t i=0;i<txbits.nbits;i++)
							txbits.data[i]=txbits.data[i+1];
						txsetp=(txsetp+(b?1:2))%3;
					}
					else
					{
						txsetp=(txsetp+2)%3;
					}
				}
			}
			double sweep=txbaud*M_PI*1.5/(double)w.sample_rate;
			double txaim=txsetp*M_PI*2/3.0;
			if((txphi>txaim-M_PI)&&(txphi<=txaim))
				txphi=min(txphi+sweep, txaim);
			else if(txphi>txaim+M_PI)
				txphi=fmod(txphi+sweep, M_PI*2);
			else if(txphi<=txaim-M_PI)
				txphi=fmod(txphi+M_PI*2-sweep, M_PI*2);
			else if((txphi>txaim)&&(txphi<=txaim+M_PI))
				txphi=max(txphi-sweep, txaim);
			else
			{
				fprintf(stderr, "Impossible txphi/aim relationship!\n");
				txphi=txaim;
			}
			double txmag=cos(M_PI/3)/cos(fmod(txphi, M_PI*2/3.0)-M_PI/3.0);
			double ft=t*2*M_PI*txcentre/w.sample_rate;
			double tx=cos(ft+txphi)*txmag/3.0;
			if(wzero) tx+=0.5;
			write_sample(w, stdout, tx*(1<<w.bits_per_sample)*.8);
			if(moni) si=tx*(1<<w.bits_per_sample)*.6-wzero;
		}
		else
			write_sample(w, stdout, wzero);
		if(wzero) si<<=1;
		double sv=si/(double)(1<<w.bits_per_sample);
		double phi=t*2*M_PI*(truif-centre)/w.sample_rate;
		fftin[t%blklen]=sv*(cos(phi)+I*sin(phi))*am/5.0;
		if(!(t%speclen))
		{
			fftw_execute(sp_p);
			SDL_FillRect(g_spectro_img, &(SDL_Rect){0, 0, 160, 60}, SDL_MapRGB(g_spectro_img->format, SPEC_BG.r, SPEC_BG.g, SPEC_BG.b));
			for(unsigned int h=1;h<9;h++)
			{
				unsigned int x=floor(h*100/spec_hpp)-20;
				line(g_spectro_img, x, 0, x, 59, (atg_colour){31, 31, 31, ATG_ALPHA_OPAQUE});
			}
			unsigned int rx=floor(centre/spec_hpp)-20;
			line(g_spectro_img, rx, 0, rx, 59, (atg_colour){0, 47, 0, ATG_ALPHA_OPAQUE});
			unsigned int tx=floor(txcentre/spec_hpp)-20;
			line(g_spectro_img, tx, 0, tx, 59, (atg_colour){63, 0, 0, ATG_ALPHA_OPAQUE});
			for(unsigned int j=0;j<160;j++)
			{
				for(unsigned int k=1;k<8;k++)
					pset(g_spectro_img, j, spec_pt[(spec_which+k)%8][j], (atg_colour){k*20, k*20, 0, ATG_ALPHA_OPAQUE});
				double mag=4*sqrt(cabs(specout[j+20]));
				pset(g_spectro_img, j, spec_pt[spec_which][j]=max(59-mag, 0), (atg_colour){255, 255, 0, ATG_ALPHA_OPAQUE});
			}
			spec_which=(spec_which+1)%8;
		}
		specin[t%speclen]=sv;
	}
	fprintf(stderr, "\n");
	return(0);
}

int pset(SDL_Surface *s, unsigned int x, unsigned int y, atg_colour c)
{
	if(!s)
		return(1);
	if((x>=(unsigned int)s->w)||(y>=(unsigned int)s->h))
		return(2);
	size_t s_off = (y*s->pitch) + (x*s->format->BytesPerPixel);
	uint32_t pixval = SDL_MapRGBA(s->format, c.r, c.g, c.b, c.a);
	*(uint32_t *)((char *)s->pixels + s_off)=pixval;
	return(0);
}

int line(SDL_Surface *s, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1, atg_colour c)
{
	// Bresenham's line algorithm, based on http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	int e=0;
	if((e=pset(s, x0, y0, c)))
		return(e);
	bool steep = abs(y1 - y0) > abs(x1 - x0);
	int tmp;
	if(steep)
	{
		tmp=x0;x0=y0;y0=tmp;
		tmp=x1;x1=y1;y1=tmp;
	}
	if(x0>x1)
	{
		tmp=x0;x0=x1;x1=tmp;
		tmp=y0;y0=y1;y1=tmp;
	}
	int dx=x1-x0,dy=abs(y1-y0);
	int ey=dx>>1;
	int dely=(y0<y1?1:-1),y=y0;
	for(int x=x0;x<(int)x1;x++)
	{
		if((e=pset(s, steep?y:x, steep?x:y, c)))
			return(e);
		ey-=dy;
		if(ey<0)
		{
			y+=dely;
			ey+=dx;
		}
	}
	return(0);
}

void ztoxy(fftw_complex z, double gsf, int *x, int *y)
{
	if(x) *x=creal(z)*gsf+60;
	if(y) *y=cimag(z)*gsf+60;
}

/* Data for the selector */
const char *sel_labels[4]={"10","30","150","750"};
atg_colour sel_colours[4]={{31, 31, 95, 0}, {95, 31, 31, 0}, {95, 95, 15, 0}, {31, 159, 31, 0}};

/* Prototype for the selector renderer */
SDL_Surface *selector_render_callback(const struct atg_element *e);

/* Prototype for the click handler */
void selector_match_click_callback(struct atg_event_list *list, atg_element *element, SDL_MouseButtonEvent button, unsigned int xoff, unsigned int yoff);

/* Function to create a custom 'selector' widget, which behaves like a radiobutton list */
atg_element *create_selector(unsigned int *sel)
{
	atg_element *rv=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){7, 7, 7, ATG_ALPHA_OPAQUE}); /* Start with an atg_box */
	if(!rv) return(NULL);
	rv->type=ATG_CUSTOM; /* Mark it as a custom widget, so that our callbacks will be used */
	rv->render_callback=selector_render_callback; /* Connect up our renderer callback */
	rv->match_click_callback=selector_match_click_callback; /* Connect up our click-handling callback */
	atg_box *b=rv->elem.box;
	if(!b)
	{
		atg_free_element(rv);
		return(NULL);
	}
	for(unsigned int i=0;i<4;i++) /* The widget is a row of four buttons */
	{
		atg_colour fg=sel_colours[i];
		/* Create the button */
		atg_element *btn=atg_create_element_button(sel_labels[i], fg, (atg_colour){7, 7, 7, ATG_ALPHA_OPAQUE});
		if(!btn)
		{
			atg_free_element(rv);
			return(NULL);
		}
		/* Pack it into the box */
		if(atg_pack_element(b, btn))
		{
			atg_free_element(rv);
			return(NULL);
		}
	}
	rv->userdata=sel; /* sel stores the currently selected value */
	return(rv);
}

/* Function to render the 'selector' widget */
SDL_Surface *selector_render_callback(const struct atg_element *e)
{
	if(!e) return(NULL);
	if(!(e->type==ATG_CUSTOM)) return(NULL);
	atg_box *b=e->elem.box;
	if(!b) return(NULL);
	if(!b->elems) return(NULL);
	/* Set the background colours */
	for(unsigned int i=0;i<b->nelems;i++)
	{
		if(e->userdata)
		{
			if(*(unsigned int *)e->userdata==i)
				b->elems[i]->elem.button->content->bgcolour=(atg_colour){159, 159, 159, ATG_ALPHA_OPAQUE};
			else
				b->elems[i]->elem.button->content->bgcolour=(atg_colour){7, 7, 7, ATG_ALPHA_OPAQUE};
		}
		else
			b->elems[i]->elem.button->content->bgcolour=(atg_colour){7, 7, 7, ATG_ALPHA_OPAQUE};
	}
	/* Hand off the actual rendering to atg_render_box */
	return(atg_render_box(e));
}

/* Function to handle clicks within the 'selector' widget */
void selector_match_click_callback(struct atg_event_list *list, atg_element *element, SDL_MouseButtonEvent button, unsigned int xoff, unsigned int yoff)
{
	atg_box *b=element->elem.box;
	if(!b->elems) return;
	struct atg_event_list sub_list={.list=NULL, .last=NULL}; /* Sub-list to catch all the events generated by our child elements */
	for(unsigned int i=0;i<b->nelems;i++) /* For each child element... */
		atg__match_click_recursive(&sub_list, b->elems[i], button, xoff+element->display.x, yoff+element->display.y); /* ...pass it the click and let it generate trigger events into our sub-list */
	unsigned int oldsel=0;
	if(element->userdata) oldsel=*(unsigned int *)element->userdata;
	while(sub_list.list) /* Iterate over the sub-list */
	{
		atg_event event=sub_list.list->event;
		if(event.type==ATG_EV_TRIGGER)
		/* We're only interested in trigger events */
		{
			if(event.event.trigger.button==ATG_MB_LEFT)
			/* Left-click on a button selects that value */
			{
				for(unsigned int i=0;i<b->nelems;i++)
				{
					if(event.event.trigger.e==b->elems[i])
					{
						if(element->userdata) *(unsigned int *)element->userdata=i;
					}
				}
			}
			else if(event.event.trigger.button==ATG_MB_SCROLLDN)
			/* Scrolling over the selector cycles through the values */
			{
				if(element->userdata) *(unsigned int *)element->userdata=(1+*(unsigned int *)element->userdata)%b->nelems;
			}
			else if(event.event.trigger.button==ATG_MB_SCROLLUP)
			/* Cycle in the opposite direction */
			{
				if(element->userdata) *(unsigned int *)element->userdata=(b->nelems-1+*(unsigned int *)element->userdata)%b->nelems;
			}
		}
		/* Get the next element from the sub-list */
		atg__event_list *next=sub_list.list->next;
		free(sub_list.list);
		sub_list.list=next;
	}
	if(element->userdata&&(*(unsigned int *)element->userdata!=oldsel))
	{
		atg__push_event(list, (atg_event){.type=ATG_EV_VALUE, .event.value=(atg_ev_value){.e=element, .value=*(unsigned int *)element->userdata}});
	}
}
