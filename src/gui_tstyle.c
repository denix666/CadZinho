#include "gui_tstyle.h"

/* compare text styles names - for sort functions */
int cmp_sty_name(const void * a, const void * b) {
	char *name1, *name2;
	char copy1[DXF_MAX_CHARS], copy2[DXF_MAX_CHARS];
	
	dxf_tstyle *sty1 = ((struct sort_by_idx *)a)->data;
	dxf_tstyle *sty2 = ((struct sort_by_idx *)b)->data;
	/* copy strings for secure manipulation */
	strncpy(copy1, strpool_cstr2( &name_pool, sty1->name), DXF_MAX_CHARS);
	strncpy(copy2,  strpool_cstr2( &name_pool, sty2->name), DXF_MAX_CHARS);
	/* remove trailing spaces */
	name1 = trimwhitespace(copy1);
	name2 = trimwhitespace(copy2);
	/* change to upper case */
	str_upp(name1);
	str_upp(name2);
	return (strncmp(name1, name2, DXF_MAX_CHARS));
}
/* compare text styles names - reverse mode - for sort functions */
int cmp_sty_name_rev(const void * a, const void * b) {
	return (-cmp_sty_name(a, b));
}
/* compare text styles font names - for sort functions */
int cmp_sty_font(const void * a, const void * b) {
	char *name1, *name2;
	char copy1[DXF_MAX_CHARS], copy2[DXF_MAX_CHARS];
	int ret;
	
	dxf_tstyle *sty1 = ((struct sort_by_idx *)a)->data;
	dxf_tstyle *sty2 = ((struct sort_by_idx *)b)->data;
	/* copy strings for secure manipulation */
	strncpy(copy1, strpool_cstr2( &value_pool, sty1->file), DXF_MAX_CHARS);
	strncpy(copy2, strpool_cstr2( &value_pool, sty2->file), DXF_MAX_CHARS);
	/* remove trailing spaces */
	name1 = trimwhitespace(copy1);
	name2 = trimwhitespace(copy2);
	/* change to upper case */
	str_upp(name1);
	str_upp(name2);
	ret = strncmp(name1, name2, DXF_MAX_CHARS);
	if (ret == 0) { /* in case the font is the same, sort by style name */
		return cmp_sty_name(a, b);
	}
	return ret;
}
/* compare text styles font names - reverse mode - for sort functions */
int cmp_sty_font_rev(const void * a, const void * b) {
	return  -cmp_sty_font(a, b);
}

/* compare text styles width factor - for sort functions */
int cmp_sty_wf(const void * a, const void * b) {
	
	dxf_tstyle *sty1 = ((struct sort_by_idx *)a)->data;
	dxf_tstyle *sty2 = ((struct sort_by_idx *)b)->data;
	
	if (sty1->width_f > sty2->width_f) return 1;
	else if (sty1->width_f < sty2->width_f) return -1;
	else return 0;
}
/* compare text styles width factor - reverse mode - for sort functions */
int cmp_sty_wf_rev(const void * a, const void * b) {
	return  -cmp_sty_wf(a, b);
}

/* compare text styles fixed height - for sort functions */
int cmp_sty_fh(const void * a, const void * b) {
	
	dxf_tstyle *sty1 = ((struct sort_by_idx *)a)->data;
	dxf_tstyle *sty2 = ((struct sort_by_idx *)b)->data;
	
	if (sty1->fixed_h > sty2->fixed_h) return 1;
	else if (sty1->fixed_h < sty2->fixed_h) return -1;
	else return 0;
}
/* compare text styles fixed height - reverse mode - for sort functions */
int cmp_sty_fh_rev(const void * a, const void * b) {
	return  -cmp_sty_fh(a, b);
}

/* compare text styles oblique angle - for sort functions */
int cmp_sty_oa(const void * a, const void * b) {
	
	dxf_tstyle *sty1 = ((struct sort_by_idx *)a)->data;
	dxf_tstyle *sty2 = ((struct sort_by_idx *)b)->data;
	
	if (sty1->oblique > sty2->oblique) return 1;
	else if (sty1->oblique < sty2->oblique) return -1;
	else return 0;
}
/* compare text styles oblique angle - reverse mode - for sort functions */
int cmp_sty_oa_rev(const void * a, const void * b) {
	return  -cmp_sty_oa(a, b);
}

int t_sty_rename(dxf_drawing *drawing, int idx, char *name){
	/* Rename text style - change the STYLE structure and the DXF entities wich uses this text style */
	int ok = 0, i;
	dxf_node *current, *prev, *obj = NULL, *list[2], *sty_obj;
	//char *new_name = trimwhitespace(name); /* remove trailing spaces*/
  STRPOOL_U64 new_name = strpool_inject( &name_pool, (char const*) name, strlen(name) );
	
	list[0] = NULL; list[1] = NULL;
	if (drawing){
		/* look for entities in BLOCKS and ENTITIES sections */
		list[0] = drawing->ents;
		list[1] = drawing->blks;
	}
	else return 0;
	
	for (i = 0; i< 2; i++){
		obj = list[i];
		current = obj;
		while (current){ /* sweep current section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
				/* Look for DXF code group 7, text style name */
				sty_obj = dxf_find_attr2(current, 7);
				if (sty_obj){
					/* verify style name in entity*/
          if(sty_obj->value.str == drawing->text_styles[idx].name){
						/* change if match */
						dxf_attr_change(current, 7,
              (void *) strpool_cstr2( &name_pool, new_name));
					}
				}
				
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/
			
			if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
				current = NULL;
				break;
			}

			/* ============================================================= */
			while (current == NULL){
				/* end of list sweeping */
				if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
					
					current = NULL;
					break;
				}
				/* try to back in structure hierarchy */
				prev = prev->master;
				if (prev){ /* up in structure */
					/* try to continue on previous point in structure */
					current = prev->next;
					
				}
				else{ /* stop the search if structure ends */
					current = NULL;
					break;
				}
			}
		}
	}
	/* change style name in DXF structure */
	dxf_attr_change(drawing->text_styles[idx].obj, 2,
    (void *) strpool_cstr2( &name_pool, new_name));
	/* change in gui */
  drawing->text_styles[idx].name = new_name;
	
	return ok;
}


int t_sty_use(dxf_drawing *drawing){
	/* Update text styles in use */
	int ok = 0, i, idx;
	dxf_node *current, *prev, *obj = NULL, *list[4], *sty_obj;
	
	list[0] = NULL; list[1] = NULL; list[2] = NULL; list[3] = NULL;
	if (drawing){
		/* look for entities in BLOCKS and ENTITIES sections */
		list[0] = drawing->ents;
		list[1] = drawing->blks;
    list[2] = drawing->t_ltype;
    list[3] = drawing->t_dimst;
	}
	else return 0;
	
	/* clear all styles */
	for (i = 0; i < drawing->num_tstyles; i++){
		drawing->text_styles[i].num_el = 0;
	}
	
	for (i = 0; i < 4; i++){
		obj = list[i];
		current = obj;
		while (current){ /* sweep current section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
        if (obj == drawing->ents || obj == drawing->blks){ /* entities */
          /* Look for DXF code group 7, text style name */
          sty_obj = dxf_find_attr2(current, 7);
          if (sty_obj){
            /* look for text style index */
            idx = dxf_tstyle_idx(drawing, sty_obj->value.str, 0);
            /* increment elements wich uses its style*/
            drawing->text_styles[idx].num_el++;
          }
				}
        else if (obj == drawing->t_ltype || obj == drawing->t_dimst){ /* tables */
          sty_obj = dxf_find_attr2(current, 340); /* get element's style handle */
          if (sty_obj){
            
            long int id = strtol(strpool_cstr2( &value_pool, sty_obj->value.str), NULL, 16);
            /* look for correspondent style object */
            sty_obj = dxf_find_handle(drawing->t_style, id);
            if (sty_obj){
              dxf_node * file = dxf_find_attr2(sty_obj, 3); /* get style file */
              sty_obj = dxf_find_attr2(sty_obj, 2); /* get style name */
              if (sty_obj && file){
                idx = dxf_tstyle_idx(drawing, sty_obj->value.str, file->value.str);
                /* increment elements wich uses its style*/
                drawing->text_styles[idx].num_el++;
              }
            }
          }
        }
				
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/
			
			if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
				current = NULL;
				break;
			}

			/* ============================================================= */
			while (current == NULL){
				/* end of list sweeping */
				if ((prev == NULL) || (prev == obj)){ /* stop the search if back on initial entity */
					
					current = NULL;
					break;
				}
				/* try to back in structure hierarchy */
				prev = prev->master;
				if (prev){ /* up in structure */
					/* try to continue on previous point in structure */
					current = prev->next;
					
				}
				else{ /* stop the search if structure ends */
					current = NULL;
					break;
				}
			}
		}
	}
	
	return ok;
}


int tstyles_mng (gui_obj *gui){
	/* window to manage text styles in drawing
	User can create, edit parameters or remove styles */
	int i, show_tstyle_mng = 1;
	static int show_edit = 0, show_name = 0, show_fonts = 0;
	static int sel_t_sty, t_sty_idx, sel_font, edit_sty = 0;
	
	static struct sort_by_idx sort_t_sty[DXF_MAX_FONTS];
	static char sty_name[DXF_MAX_CHARS] = "";
	static char sty_font[DXF_MAX_CHARS] = "";
	static char sty_w_fac[64] = "";
	static char sty_fixed_h[64] = "";
	static char sty_o_ang[64] = "";
	
	static int shape = 0, vertical = 0, xref = 0, xref_ok = 0, backward = 0, upside = 0;
	
	dxf_tstyle *t_sty = gui->drawing->text_styles;
	
	gui->next_win_x += gui->next_win_w + 3;
	//gui->next_win_y += gui->next_win_h + 3;
	gui->next_win_w = 870;
	gui->next_win_h = 310;
	
	int num_tstyles = gui->drawing->num_tstyles;
	
	if (sel_t_sty >= num_tstyles) sel_t_sty = 0;
	
	static struct sort_by_idx sort_tstyle[DXF_MAX_FONTS];
	
	static enum sort {
		UNSORTED,
		BY_NAME,
		BY_FONT,
		BY_WIDTH,
		BY_HEIGHT,
		BY_ANGLE
	} sorted = UNSORTED;
	static int sort_reverse = 0;
	
	if (nk_begin(gui->ctx, _l("Text Styles Manager"), nk_rect(gui->next_win_x, gui->next_win_y, gui->next_win_w, gui->next_win_h),
	NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
	NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE)){
		
		struct nk_style_button *sel_type;
		
		t_sty = gui->drawing->text_styles;
		
		nk_layout_row_dynamic(gui->ctx, 32, 1);
		if (nk_group_begin(gui->ctx, "tstyle_head", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
			nk_layout_row(gui->ctx, NK_STATIC, 22, 8, (float[]){175, 175, 175, 50, 50, 50, 50, 50});
			/* table head -  buttons to sort the list */
			/* name */
			if (sorted == BY_NAME){
				if (sort_reverse){
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_DOWN, _l("Name"), NK_TEXT_CENTERED)){
						sorted = UNSORTED;
						sort_reverse = 0;
					}
				} else {
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_UP, _l("Name"), NK_TEXT_CENTERED)){
						sort_reverse = 1;
					}
				}
			}else if (nk_button_label(gui->ctx, _l("Name"))){
				sorted = BY_NAME;
				sort_reverse = 0;
			}
			/* font */
			if (sorted == BY_FONT){
				if (sort_reverse){
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_DOWN, _l("Font"), NK_TEXT_CENTERED)){
						sorted = UNSORTED;
						sort_reverse = 0;
					}
				} else {
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_UP, _l("Font"), NK_TEXT_CENTERED)){
						sort_reverse = 1;
					}
				}
			}else if (nk_button_label(gui->ctx, _l("Font"))){
				sorted = BY_FONT;
				sort_reverse = 0;
			}
			if (nk_button_label(gui->ctx, _l("Subst"))){
				
			}
			/* Width */
			if (sorted == BY_WIDTH){
				if (sort_reverse){
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_DOWN, _l("Width"), NK_TEXT_CENTERED)){
						sorted = UNSORTED;
						sort_reverse = 0;
					}
				} else {
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_UP, _l("Width"), NK_TEXT_CENTERED)){
						sort_reverse = 1;
					}
				}
			}else if (nk_button_label(gui->ctx, _l("Width"))){
				sorted = BY_WIDTH;
				sort_reverse = 0;
			}
			/* fixed height */
			if (sorted == BY_HEIGHT){
				if (sort_reverse){
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_DOWN, _l("Fix H"), NK_TEXT_CENTERED)){
						sorted = UNSORTED;
						sort_reverse = 0;
					}
				} else {
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_UP, _l("Fix H"), NK_TEXT_CENTERED)){
						sort_reverse = 1;
					}
				}
			}else if (nk_button_label(gui->ctx, _l("Fix H"))){
				sorted = BY_HEIGHT;
				sort_reverse = 0;
			}
			/* Oblique angle */
			if (sorted == BY_ANGLE){
				if (sort_reverse){
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_DOWN, _l("Angle"), NK_TEXT_CENTERED)){
						sorted = UNSORTED;
						sort_reverse = 0;
					}
				} else {
					if (nk_button_symbol_label(gui->ctx, NK_SYMBOL_TRIANGLE_UP, _l("Angle"), NK_TEXT_CENTERED)){
						sort_reverse = 1;
					}
				}
			}else if (nk_button_label(gui->ctx, _l("Angle"))){
				sorted = BY_ANGLE;
				sort_reverse = 0;
			}
			
			if (nk_button_label(gui->ctx, _l("Flags"))){
				
			}
			
			if (nk_button_label(gui->ctx, _l("Used"))){
				
			}
			
			nk_group_end(gui->ctx);
		}
		nk_layout_row_dynamic(gui->ctx, 200, 1);
		if (nk_group_begin(gui->ctx, "tstyle_prop", NK_WINDOW_BORDER)) {
			
			/* sorting functions */
			/* initialize the list to be sorted*/
			for (i = 0; i < num_tstyles; i++){
				sort_tstyle[i].idx = i;
				sort_tstyle[i].data = &(t_sty[i]);
			}
			
			/* identify and apply the sorting criteria */
			if (sorted == BY_NAME){
				if(!sort_reverse)
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_name);
				else
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_name_rev);
			}
			else if (sorted == BY_FONT){
				if(!sort_reverse)
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_font);
				else
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_font_rev);
			}
			else if (sorted == BY_WIDTH){
				if(!sort_reverse)
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_wf);
				else
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_wf_rev);
			}
			else if (sorted == BY_HEIGHT){
				if(!sort_reverse)
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_fh);
				else
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_fh_rev);
			}
			else if (sorted == BY_ANGLE){
				if(!sort_reverse)
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_oa);
				else
					qsort(sort_tstyle, num_tstyles, sizeof(struct sort_by_idx), cmp_sty_oa_rev);
			}
			
			/* update the text styles in use */
			t_sty_use(gui->drawing);
			
			/* show text style list of drawing*/
			nk_layout_row(gui->ctx, NK_STATIC, 22, 8, (float[]){175, 175, 175, 50, 50, 50, 50, 50});
			char txt[DXF_MAX_CHARS];
			for (i = 0; i < num_tstyles; i++){
				t_sty_idx = sort_tstyle[i].idx;
        if (!t_sty[t_sty_idx].name) continue; /* show only the named styles */
        
        /* hilite the selected text style in list */
				sel_type = &gui->b_icon_unsel;
				if (sel_t_sty == t_sty_idx) sel_type = &gui->b_icon_sel;
				
				/* show current text style name */
				if (nk_button_label_styled(gui->ctx, sel_type,
          strpool_cstr2( &name_pool, t_sty[t_sty_idx].name))){
					sel_t_sty = t_sty_idx; /* select current text style */
				}
				
				/* show font file */
				if (nk_button_label_styled(gui->ctx, sel_type,
          strpool_cstr2( &value_pool, t_sty[t_sty_idx].file)))
          sel_t_sty = t_sty_idx; /* select current text style */
          
				
				/* show if font was substituted in case of unavailability */
				if (nk_button_label_styled(gui->ctx, sel_type,
          strpool_cstr2( &value_pool, t_sty[t_sty_idx].subst_file)))
          sel_t_sty = t_sty_idx; /* select current text style */
				
				/* show width factor */
				snprintf(txt, DXF_MAX_CHARS, "%0.2f", t_sty[t_sty_idx].width_f);
				if (nk_button_label_styled(gui->ctx, sel_type, txt)) sel_t_sty = t_sty_idx; /* select current text style */
				
				/* show fixed height */
				snprintf(txt, DXF_MAX_CHARS, "%0.2f", t_sty[t_sty_idx].fixed_h);
				if (nk_button_label_styled(gui->ctx, sel_type, txt)) sel_t_sty = t_sty_idx; /* select current text style */
				
				/* show oblique angle */
				snprintf(txt, DXF_MAX_CHARS, "%0.2f", t_sty[t_sty_idx].oblique);
				if (nk_button_label_styled(gui->ctx, sel_type, txt)) sel_t_sty = t_sty_idx; /* select current text style */
				
				/* show flags status of current text style */
				snprintf(txt, DXF_MAX_CHARS, "---");
				if (t_sty[t_sty_idx].flags1 & 4) txt[0] = 'V'; /* vertical text */
				if (t_sty[t_sty_idx].flags2 & 2) txt[1] = 'B'; /* backward text - flip in X coord */
				if (t_sty[t_sty_idx].flags2 & 4) txt[2] = 'U'; /* upside down text - flip in Y coord */
				if (nk_button_label_styled(gui->ctx, sel_type, txt)) sel_t_sty = t_sty_idx; /* select current text style */
				
				/* show if current text style is in use */
				if (t_sty[t_sty_idx].num_el) snprintf(txt, DXF_MAX_CHARS, "x");
				else snprintf(txt, DXF_MAX_CHARS, " ");
				if (nk_button_label_styled(gui->ctx, sel_type, txt)) sel_t_sty = t_sty_idx; /* select current text style */
				
			}
			nk_group_end(gui->ctx);
		}
		
		nk_layout_row_dynamic(gui->ctx, 20, 5);
		if (nk_button_label(gui->ctx, _l("Create"))){
			/* show popup window for entry new text style name */
			show_name = 1;
			sty_name[0] = 0;
		}
		if ((nk_button_label(gui->ctx, _l("Edit"))) && (sel_t_sty >= 0)){
			show_edit = 1; /* show edit window */
			/* initialize variables for edit window with selected text style parameters*/
			strncpy(sty_name,
        strpool_cstr2( &name_pool, t_sty[sel_t_sty].name), DXF_MAX_CHARS);
			strncpy(sty_font,
        strpool_cstr2( &value_pool, t_sty[sel_t_sty].file), DXF_MAX_CHARS);
			
			snprintf(sty_w_fac, 63, "%f", t_sty[sel_t_sty].width_f);
			snprintf(sty_fixed_h, 63, "%f", t_sty[sel_t_sty].fixed_h);
			snprintf(sty_o_ang, 63, "%f", t_sty[sel_t_sty].oblique);
			
			vertical = (t_sty[sel_t_sty].flags1 & 4) != 0;
			backward = (t_sty[sel_t_sty].flags2 & 2) != 0;
			upside = (t_sty[sel_t_sty].flags2 & 4) != 0;
			
			edit_sty = sel_t_sty;
			
		}
		if ((nk_button_label(gui->ctx, _l("Remove"))) && (sel_t_sty >= 0)){
			
			/* preserve STANDARD text style from remove */
      if (t_sty[sel_t_sty].name == strpool_inject( &name_pool, "STANDARD", strlen("STANDARD") )){
				snprintf(gui->log_msg, 63, _l("Error: Don't remove Standard Style"));
			}
			else{
				/*check if text style is used */
				t_sty_use(gui->drawing); /* update for sure */
				if (t_sty[sel_t_sty].num_el){
					snprintf(gui->log_msg, 63, _l("Error: Don't remove Style in use"));
				}
				else{ /* if allowed, proceed to remove */
					do_add_entry(&gui->list_do, _l("Remove Style"));
					do_add_item(gui->list_do.current, t_sty[sel_t_sty].obj, NULL);
					dxf_obj_subst(t_sty[sel_t_sty].obj, NULL);
					sel_t_sty = 0;
					dxf_tstyles_assemb (gui->drawing); /*update drawing*/
				}
			}
		}
		
		nk_label(gui->ctx, " ", NK_TEXT_RIGHT); /*label only for simple space layout */
		
		if (nk_button_label(gui->ctx, _l("Fonts"))){
			/* show fonts manager */
			show_fonts = 1;
		}
		
		if ((show_name)){ /* popup window to creat a new text style */
			if (nk_popup_begin(gui->ctx, NK_POPUP_STATIC, _l("Style Name"), NK_WINDOW_CLOSABLE, nk_rect(10, 20, 220, 100))){
				
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				/* set focus to edit */
				nk_edit_focus(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT);
				/* new text style name*/
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, sty_name, DXF_MAX_CHARS, nk_filter_default);
				
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("OK"))){
					/* try to create a new text style */
					if (!dxf_new_tstyle (gui->drawing, sty_name)){
						snprintf(gui->log_msg, 63, _l("Error: Text Style already exists"));
					}
					else {
						/* success - close popup */
						nk_popup_close(gui->ctx);
						show_name = 0;
					}
				}
				if (nk_button_label(gui->ctx, _l("Cancel"))){
					nk_popup_close(gui->ctx);
					show_name = 0;
				}
				nk_popup_end(gui->ctx);
			} else {
				show_name = 0;
			}
		}
		
		
	} else {
		show_tstyle_mng = 0;
	}
	nk_end(gui->ctx);
	
	if ((show_edit)){
		/* edit window - allow modifications on parameters of selected text style */
		if (nk_begin(gui->ctx, _l("Edit Text Style"), nk_rect(gui->next_win_x, gui->next_win_y + gui->next_win_h + 3, 330, 220), NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_CLOSABLE)){
			static int show_app_file = 0;
			
			nk_layout_row(gui->ctx, NK_STATIC, 20, 2, (float[]){100, 200});
			
			/* -------------------- name setting ------------------*/
			nk_label(gui->ctx, _l("Name:"), NK_TEXT_RIGHT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, sty_name, DXF_MAX_CHARS, nk_filter_default);
			
			/* -------------------- font setting ------------------*/
			nk_label(gui->ctx, _l("Font:"), NK_TEXT_RIGHT);
			sel_font = -1;
			if (nk_combo_begin_label(gui->ctx,  sty_font, nk_vec2(220,200))){
				
				/* get filename from available loaded fonts */
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				int j = 0;
				list_node *curr_node = NULL;
				while (curr_node = list_get_idx(gui->font_list, j)){
					
					if (!(curr_node->data)) continue;
					struct tfont *curr_font = curr_node->data;
					
					if (nk_button_label(gui->ctx, curr_font->name)){
						sel_font = j; /* select current font */
						strncpy(sty_font, curr_font->name, DXF_MAX_CHARS);
						nk_combo_close(gui->ctx);
					}
					j++;
				}
				nk_combo_end(gui->ctx);
			}
			/* -------------------- width setting ------------------*/
			nk_layout_row(gui->ctx, NK_STATIC, 20, 2, (float[]){100, 100});
			nk_label(gui->ctx, _l("Width factor:"), NK_TEXT_RIGHT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, sty_w_fac, 63, nk_filter_float);
			
			/* -------------------- fixed heigth setting ------------------*/
			nk_label(gui->ctx, _l("Fixed height:"), NK_TEXT_RIGHT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, sty_fixed_h, 63, nk_filter_float);
			
			/* -------------------- oblique angle setting ------------------*/
			nk_label(gui->ctx, _l("Oblique angle:"), NK_TEXT_RIGHT);
			nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT, sty_o_ang, 63, nk_filter_float);
			
			nk_layout_row_dynamic(gui->ctx, 20, 3);
			
			/* -------------------- flags ------------------*/
			//nk_checkbox_label(gui->ctx, _l("Shape"), &shape);
			nk_checkbox_label(gui->ctx, _l("Vertical"), &vertical);
			nk_checkbox_label(gui->ctx, _l("Backward"), &backward);
			nk_checkbox_label(gui->ctx, _l("Upside down"), &upside);
				
				
			nk_layout_row(gui->ctx, NK_STATIC, 20, 2, (float[]){100, 100});
			if (nk_button_label(gui->ctx, _l("OK"))){
				
				
				/* verify if name is duplicated */
				int sty_exist = 0;
        STRPOOL_U64 new_name = strpool_inject( &name_pool, (char const*) sty_name, strlen(sty_name) );
        STRPOOL_U64 std = strpool_inject( &name_pool, "STANDARD", strlen("STANDARD") );
				for (i = 0; i < num_tstyles; i++){
					if (i == edit_sty) { /*if current text style */
						/* preserve STANDARD text style from rename*/
            if (new_name == std){
              sty_exist = 2;
							break;
						}
					}
					else{ /* verifify duplicated */
            if (t_sty[i].name == new_name) {
              sty_exist = 1;
              break;
            }
					}
				}
				
				if (!sty_exist){ /*proceed to rename, if no exists duplicated name*/
					
					/*--------- Style name  -----------*/
					/*verify if was renamed */
          int renamed = t_sty[sel_t_sty].name != new_name;
					if (renamed) {
						/* change the STYLE structure and the DXF entities wich uses this text style */
						t_sty_rename(gui->drawing, edit_sty,
              (char *) strpool_cstr2( &name_pool, new_name));
					}
					
					/*--------- Style font file  -----------*/
					/* change font setting in gui */
          t_sty[edit_sty].file = strpool_inject( &value_pool, sty_font, strlen(sty_font) );
					/* change font setting in DXF structure */
					dxf_attr_change(t_sty[edit_sty].obj, 3, sty_font);
					
					/*--------- Style numeric parameters -----------*/
					/* change setting in gui */
					t_sty[edit_sty].width_f = atof(sty_w_fac);
					t_sty[edit_sty].fixed_h = atof(sty_fixed_h);
					t_sty[edit_sty].oblique = atof(sty_o_ang);
					
					/* change setting in DXF structure */
					dxf_attr_change(t_sty[edit_sty].obj, 41, &(t_sty[edit_sty].width_f));
					dxf_attr_change(t_sty[edit_sty].obj, 40, &(t_sty[edit_sty].fixed_h));
					dxf_attr_change(t_sty[edit_sty].obj, 50, &(t_sty[edit_sty].oblique));
					
					/*--------- Style flags parameters -----------*/
					/* change setting in gui */
					if(vertical) t_sty[sel_t_sty].flags1 |= 1UL << 2;
					else t_sty[sel_t_sty].flags1 &= ~(1UL << 2);
					if(backward) t_sty[sel_t_sty].flags2 |= 1UL << 1;
					else t_sty[sel_t_sty].flags2 &= ~(1UL << 1);
					if(upside) t_sty[sel_t_sty].flags2 |= 1UL << 2;
					else t_sty[sel_t_sty].flags2 &= ~(1UL << 2);
					
					/* change setting in DXF structure */
					dxf_attr_change(t_sty[edit_sty].obj, 70, &(t_sty[edit_sty].flags1));
					dxf_attr_change(t_sty[edit_sty].obj, 71, &(t_sty[edit_sty].flags2));
					
					/* update settings in drawing */
					gui_tstyle(gui);
					
					show_edit = 0;
				}
				else{ /* show error messages */
					if (sty_exist == 1) snprintf(gui->log_msg, 63, _l("Error: Duplicated Text Style"));
					else if (sty_exist == 2) snprintf(gui->log_msg, 63, _l("Error: STANDARD style can't be renamed"));
				}
			}
			if (nk_button_label(gui->ctx, _l("Cancel"))){
				show_edit = 0;
			}
			
		} else {
			show_edit = 0;
		}
		nk_end(gui->ctx);
	}
	
	if ((show_fonts)){
		/* show fonts */
		static int show_app_file = 0, sel_f_idx = 0, prev = -1;
		if (nk_begin(gui->ctx, _l("Manage Fonts"), nk_rect(gui->next_win_x + 235, gui->next_win_y + 80, 400, 260), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE)){
			struct tfont * selected_font = NULL;
			
			struct nk_style_button *sel_type;
			
			nk_layout_row(gui->ctx, NK_STATIC, 190, 2, (float[]){167, 193});
			
			if (nk_group_begin(gui->ctx, _l("Available Fonts"), NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {
				/* show loaded fonts, available for setting */
				
				sel_type = &gui->b_icon_unsel;
				if (sel_t_sty == t_sty_idx) sel_type = &gui->b_icon_sel;
				
				nk_style_push_font(gui->ctx, &(gui->alt_font_sizes[FONT_SMALL])); /* change font to tiny*/
				
				nk_layout_row_dynamic(gui->ctx, 15, 1);
				int j = 0;
				list_node *curr_node = NULL;
				while (curr_node = list_get_idx(gui->font_list, j)){
					
					if (curr_node->data) {
						struct tfont *curr_font = curr_node->data;
						/* hilite selected font */
						sel_type = &gui->b_icon_unsel;
						if (j == sel_f_idx){
							sel_type = &gui->b_icon_sel;
							selected_font = curr_font;
						}
						if (nk_button_label_styled(gui->ctx, sel_type, curr_font->name)) {
							sel_f_idx = j; /* select current font */
							selected_font = curr_font;
						}
					}
					j++;
				}
				
				if (prev != sel_f_idx){
					prev = sel_f_idx;
					bmp_color color = {.r = 255, .g = 255, .b =255, .a = 255};
					list_node *preview = list_new(NULL, FRAME_LIFE); /*graphic object of preview font */
					int ei; /*extents flag of preview */
					/* extents and zoom parameters */
					double x0, y0, x1, y1, z, z_x, z_y, o_x, o_y;
					double z0, z1;
					
					int pos1, pos2, pos3, pos4;
					
					ei = 0;
					/* get graphics preview of selected font*/
					/* 4 strings samples: Uppercase, lowercase, numbers, symblos */
					pos1 = font_parse_str(selected_font, preview, FRAME_LIFE, "GINJAL", NULL, 0);
					pos2 = font_parse_str(selected_font, preview, FRAME_LIFE, "ginjal", NULL, 0);
					pos3 = font_parse_str(selected_font, preview, FRAME_LIFE, "012345", NULL, 0);
					pos4 = font_parse_str(selected_font, preview, FRAME_LIFE, "!@#$%&", NULL, 0);
					graph_list_modify_idx(preview, 0.0, -1.4 , 1.0, 1.0, 0.0, pos1, pos1 + pos2 - 1);
					graph_list_modify_idx(preview, 0.0, -2.8 , 1.0, 1.0, 0.0, pos1 + pos2, pos1 + pos2 + pos3 - 1);
					graph_list_modify_idx(preview, 0.0, -4.2 , 1.0, 1.0, 0.0, pos1 + pos2 + pos3, pos1 + pos2 + pos3 + pos4);
					/* get extents parameters*/
					graph_list_ext(preview, &ei, &x0, &y0, &z0, &x1, &y1, &z1);
					/*change color */
					graph_list_color(preview, color);
					
					/* calcule the zoom and offset for preview */
					z_x = fabs(x1 - x0)/gui->preview[PRV_FONT]->width;
					z_y = fabs(y1 - y0)/gui->preview[PRV_FONT]->height;
					z = (z_x > z_y) ? z_x : z_y;
					if (z <= 0) z =1;
					else z = 1/(1.1 * z);
					o_x = x0 - (fabs((x1 - x0)*z - gui->preview[PRV_FONT]->width)/2)/z;
					o_y = y0 - (fabs((y1 - y0)*z - gui->preview[PRV_FONT]->height)/2)/z;
					
					/* draw graphics in current preview bitmap */
					bmp_fill(gui->preview[PRV_FONT], gui->preview[PRV_FONT]->bkg); /* clear bitmap preview */
					//graph_list_draw(preview, gui->preview[PRV_FONT], o_x, o_y, z);
					struct draw_param d_param;
	
					d_param.ofs_x = o_x;
					d_param.ofs_y = o_y;
					d_param.ofs_z = 0;
					d_param.scale = z;
					d_param.list = NULL;
					d_param.subst = NULL;
					d_param.len_subst = 0;
					d_param.inc_thick = 0;
					graph_list_draw(preview, gui->preview[PRV_FONT], d_param);
					
				}
				
				nk_style_pop_font(gui->ctx); /* return to the default font*/
				nk_group_end(gui->ctx);
			}
			if (nk_group_begin(gui->ctx, "Font_prev", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
				/* preview img */
				nk_layout_row_dynamic(gui->ctx, 175, 1);
				nk_button_image(gui->ctx,  nk_image_ptr(gui->preview[PRV_FONT]));
				
				nk_group_end(gui->ctx);
			}
			
			nk_layout_row_dynamic(gui->ctx, 20, 3);
			
			/* supported file formats */
			static const char *ext_type[] = {
				"SHP",
				"SHX",
				"TTF",
				"OTF",
				"*"
			};
      
      static char ext_descr[5][DXF_MAX_CHARS + 1];
      strncpy(ext_descr[0], _l("Shapes files (.shp)"), DXF_MAX_CHARS);
      strncpy(ext_descr[1], _l("Binary shapes file (.shx)"), DXF_MAX_CHARS);
      strncpy(ext_descr[2], _l("True type font file (.ttf)"), DXF_MAX_CHARS);
      strncpy(ext_descr[3], _l("Open font file (.otf)"), DXF_MAX_CHARS);
      strncpy(ext_descr[4], _l("All files (*)"), DXF_MAX_CHARS);
			#define FILTER_COUNT 5
			
			if (nk_button_label(gui->ctx, _l("Load font"))){
				/* load more other fonts */
				show_app_file = 1;
				/* set filter for suported output formats */
				for (i = 0; i < FILTER_COUNT; i++){
					gui->file_filter_types[i] = ext_type[i];
					gui->file_filter_descr[i] = ext_descr[i];
				}
				gui->file_filter_count = FILTER_COUNT;
				gui->filter_idx = 0;
				
				gui->show_file_br = 1;
				gui->curr_path[0] = 0;
			}
			if (show_app_file){
				if (gui->show_file_br == 2){ /* return file OK */
					/* close browser window*/
					gui->show_file_br = 0;
					show_app_file = 0;
					add_font_list(gui->font_list, gui->curr_path, gui->dflt_fonts_path);
					show_app_file = 0;
				}
			}
			
		} else {
			show_fonts = 0;
			prev = -1;
			bmp_fill(gui->preview[PRV_FONT], gui->preview[PRV_FONT]->bkg); /* clear bitmap preview */
		}
		nk_end(gui->ctx);
	}
	
	return show_tstyle_mng;
}