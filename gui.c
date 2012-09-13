#include "gui.h"
#include <atg_internals.h>
#include "strbuf.h"

const char *set_tbl[6]={
"Baud Rates",
"BW min max",
"10   1   7",
"30   1  20",
"150  4  90",
"750 40 400"};

int make_gui(gui *buf, unsigned int *bws)
{
	if(!buf) return(1);
	buf->canvas=atg_create_canvas(480, 320, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
	if(!buf->canvas)
	{
		fprintf(stderr, "atg_create_canvas failed\n");
		return(1);
	}
	SDL_WM_SetCaption("3PSK", "3PSK");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	atg_box *mainbox=buf->canvas->box;
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
	buf->g_bauds=NULL;
	buf->g_tx=NULL;
	buf->g_moni=NULL;
	buf->g_afc=NULL;
	buf->g_spl=NULL;
	buf->bwsel=bws;
	buf->g_bw=NULL;
	buf->g_spl=NULL;
	buf->g_txf=NULL;
	buf->g_rxf=NULL;
	buf->g_constel_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 120, 120, buf->canvas->surface->format->BitsPerPixel, buf->canvas->surface->format->Rmask, buf->canvas->surface->format->Gmask, buf->canvas->surface->format->Bmask, buf->canvas->surface->format->Amask);
	if(!buf->g_constel_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	buf->g_phasing_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 100, 120, buf->canvas->surface->format->BitsPerPixel, buf->canvas->surface->format->Rmask, buf->canvas->surface->format->Gmask, buf->canvas->surface->format->Bmask, buf->canvas->surface->format->Amask);
	if(!buf->g_phasing_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	buf->g_spectro_img=SDL_CreateRGBSurface(SDL_SWSURFACE, 160, 60, buf->canvas->surface->format->BitsPerPixel, buf->canvas->surface->format->Rmask, buf->canvas->surface->format->Gmask, buf->canvas->surface->format->Bmask, buf->canvas->surface->format->Amask);
	if(!buf->g_spectro_img)
	{
		fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
		return(1);
	}
	else
	{
		SDL_FillRect(buf->g_constel_img, &(SDL_Rect){0, 0, 120, 120}, SDL_MapRGB(buf->g_constel_img->format, CONS_BG.r, CONS_BG.g, CONS_BG.b));
		SDL_FillRect(buf->g_phasing_img, &(SDL_Rect){0, 0, 100, 120}, SDL_MapRGB(buf->g_phasing_img->format, PHAS_BG.r, PHAS_BG.g, PHAS_BG.b));
		SDL_FillRect(buf->g_spectro_img, &(SDL_Rect){0, 0, 160,  60}, SDL_MapRGB(buf->g_spectro_img->format, SPEC_BG.r, SPEC_BG.g, SPEC_BG.b));
		atg_box *g_db=g_decoder->elem.box;
		if(!g_db)
		{
			fprintf(stderr, "g_decoder->elem.box==NULL\n");
			return(1);
		}
		atg_element *g_constel=atg_create_element_image(buf->g_constel_img);
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
		atg_element *g_phasing=atg_create_element_image(buf->g_phasing_img);
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
		atg_element *g_tx=atg_create_element_toggle("TX", false, (atg_colour){191, 0, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_tx)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		atg_toggle *t=g_tx->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_tx->elem.toggle==NULL\n");
			return(1);
		}
		buf->g_tx=&t->state;
		g_tx->userdata="TX";
		if(atg_pack_element(bb1, g_tx))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_moni=atg_create_element_toggle("MONI", false, (atg_colour){0, 191, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_moni)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		t=g_moni->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_moni->elem.toggle==NULL\n");
			return(1);
		}
		buf->g_moni=&t->state;
		g_moni->userdata="MONI";
		if(atg_pack_element(bb1, g_moni))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_afc=atg_create_element_toggle("AFC", false, (atg_colour){191, 127, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_afc)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		t=g_afc->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_afc->elem.toggle==NULL\n");
			return(1);
		}
		buf->g_afc=&t->state;
		g_afc->userdata="AFC";
		if(atg_pack_element(bb1, g_afc))
		{
			perror("atg_pack_element");
			return(1);
		}
		atg_element *g_spl=atg_create_element_toggle("SPL", false, (atg_colour){191, 191, 0, ATG_ALPHA_OPAQUE}, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!g_spl)
		{
			fprintf(stderr, "atg_create_element_toggle failed\n");
			return(1);
		}
		t=g_spl->elem.toggle;
		if(!t)
		{
			fprintf(stderr, "g_moni->elem.toggle==NULL\n");
			return(1);
		}
		buf->g_spl=&t->state;
		g_spl->userdata="SPL";
		if(atg_pack_element(bb1, g_spl))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_bw=create_selector(bws);
		if(!buf->g_bw)
		{
			fprintf(stderr, "create_selector failed\n");
			return(1);
		}
		if(atg_pack_element(b, buf->g_bw))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_txb=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 600, 1, 0, "TXB %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!buf->g_txb)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		buf->g_txb->userdata="TXB";
		if(atg_pack_element(b, buf->g_txb))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_txf=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 200, 800, 1, 0, "TXF %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!buf->g_txf)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		buf->g_txf->userdata="TXF";
		if(atg_pack_element(b, buf->g_txf))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_rxf=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 200, 800, 1, 0, "RXF %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!buf->g_rxf)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		buf->g_rxf->userdata="RXF";
		if(atg_pack_element(b, buf->g_rxf))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_rxs=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_STEP10, 1, 64, 1, 0, "RXS %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!buf->g_rxs)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		buf->g_rxs->userdata="RXS";
		if(atg_pack_element(b, buf->g_rxs))
		{
			perror("atg_pack_element");
			return(1);
		}
		buf->g_amp=atg_create_element_spinner(ATG_SPINNER_RIGHTCLICK_TIMES2, 1, 25, 1, 0, "AMP %03d", (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE}, (atg_colour){15, 15, 15, ATG_ALPHA_OPAQUE});
		if(!buf->g_amp)
		{
			fprintf(stderr, "atg_create_element_spinner failed\n");
			return(1);
		}
		buf->g_amp->userdata="AMP";
		if(atg_pack_element(b, buf->g_amp))
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
		buf->g_bauds=g_baud_label->elem.label->text;
		if(!buf->g_bauds)
		{
			fprintf(stderr, "buf->g_bauds==NULL\n");
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
		atg_element *g_spectro=atg_create_element_image(buf->g_spectro_img);
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
		if(!(buf->outtext[i]=line->elem.label->text=malloc(OUTLINELEN+2)))
		{
			perror("malloc");
			return(1);
		}
		buf->outtext[i][0]=' ';
		buf->outtext[i][1]=0;
		if(atg_pack_element(mainbox, line))
		{
			perror("atg_pack_element");
			return(1);
		}
	}
	init_char(&buf->inr, &buf->inrl, &buf->inri);
	init_char(&buf->ing, &buf->ingl, &buf->ingi);
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
			if(!(buf->intextleft[i]=left->elem.label->text=malloc(INLINELEN+1)))
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
			if(!(buf->intextright[i]=right->elem.label->text=malloc(INLINELEN+1)))
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
	for(unsigned int i=0;i<NMACROS;i++)
	{
		buf->mline[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){23, 23, 23, ATG_ALPHA_OPAQUE});
		if(!buf->mline[i])
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		buf->mline[i]->w=buf->canvas->surface->w;
		buf->mline[i]->clickable=true;
		if(atg_pack_element(mainbox, buf->mline[i]))
		{
			perror("atg_pack_element");
			return(1);
		}
		else
		{
			atg_box *b=buf->mline[i]->elem.box;
			if(!b)
			{
				fprintf(stderr, "mline[%u]->elem.box==NULL\n", i);
				return(1);
			}
			buf->mcol[i]=&b->bgcolour;
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
			if(!(buf->macro[i]=text->elem.label->text=malloc(MACROLEN+1)))
			{
				perror("malloc");
				return(1);
			}
			buf->macro[i][0]=0;
			if(atg_pack_element(b, text))
			{
				perror("atg_pack_element");
				return(1);
			}
		}
	}
	return(0);
}

int setspinval(atg_element *spinner, int value)
{
	if(!spinner) return(1);
	atg_spinner *s=spinner->elem.spinner;
	if(!s)
	{
		fprintf(stderr, "spinner->elem.spinner==NULL\n");
		return(1);
	}
	s->value=value;
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
