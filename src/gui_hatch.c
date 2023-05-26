#include "gui_use.h"
#include <float.h>

int gui_hatch_interactive(gui_obj *gui){
	/* Initially, uses a lwpolyline (without bulge) as bondary. */
	if (gui->modal == HATCH){
		static dxf_node *new_el;
		if (!gui->hatch_sel){
			if (gui->step == 0){
				gui->free_sel = 0;
				if (gui->ev & EV_ENTER){
					/* create a new DXF lwpolyline */
					new_el = (dxf_node *) dxf_new_lwpolyline (
						gui->step_x[gui->step], gui->step_y[gui->step], 0.0, /* pt1, */
						0.0, /* bulge */
						gui->color_idx, /* color, layer */
            (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
						/* line type, line weight */
            (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
            dxf_lw[gui->lw_idx],
						0, DWG_LIFE); /* paper space */
					dxf_lwpoly_append (new_el, gui->step_x[gui->step], gui->step_y[gui->step], 0.0, 0.0, DWG_LIFE);
					dxf_attr_change_i(new_el, 70, (void *) (int[]){1}, 0);
					gui->element = new_el;
					gui->step = 1;
					gui->en_distance = 1;
					gui->draw_tmp = 1;
					gui_next_step(gui);
				}
				else if (gui->ev & EV_CANCEL){
					gui_default_modal(gui);
				}
			}
			else{
				if (gui->ev & EV_ENTER){
					gui->step_x[gui->step - 1] = gui->step_x[gui->step];
					gui->step_y[gui->step - 1] = gui->step_y[gui->step];
					
					dxf_attr_change_i(new_el, 10, &gui->step_x[gui->step], -1);
					dxf_attr_change_i(new_el, 20, &gui->step_y[gui->step], -1);
					//dxf_attr_change_i(new_el, 42, &gui->bulge, -1);
					
					new_el->obj.graphics = dxf_graph_parse(gui->drawing, new_el, 0 , 1);
					
					dxf_lwpoly_append (new_el, gui->step_x[gui->step], gui->step_y[gui->step], 0.0, 0.0, DWG_LIFE);
					gui->step = 2;
					gui_next_step(gui);
				}
				else if (gui->ev & EV_CANCEL){
					gui->draw_tmp = 0;
					if (gui->step == 2){
						dxf_lwpoly_remove (new_el, -1);
						//new_el->obj.graphics = dxf_graph_parse(gui->drawing, new_el, 0 , 0);
						//drawing_ent_append(gui->drawing, new_el);
						
						graph_obj *bound = dxf_lwpline_parse(gui->drawing, new_el, 0 , 0);
						
						struct h_pattern *curr_h;// = &(gui->list_pattern);
						struct h_family *curr_fam = gui->hatch_fam.next;
						int i = 0;
						
						double rot = 0.0, scale = 1.0;
						
						if(gui->h_type == HATCH_USER) { /* user definied simple pattern */
							strncpy(gui->list_pattern.name, "USER_DEF", DXF_MAX_CHARS);
							curr_h = &(gui->list_pattern);
							rot = 0.0;
							scale = 1.0;
						}
						else if(gui->h_type == HATCH_SOLID) { /* solid pattern */
							strncpy(gui->list_pattern.name, "SOLID", DXF_MAX_CHARS);
							curr_h = &(gui->list_pattern);
							rot = 0.0;
							scale = 1.0;
						}
						else{ /* pattern from library */
							
							/* get current family */
							curr_h = NULL;
							i = 0;
							while (curr_fam){
								if (gui->hatch_fam_idx == i){
									curr_h = curr_fam->list->next;
									break;
								}
								
								i++;
								curr_fam = curr_fam->next;
							}
							
							/* get current hatch pattern */
							i = 0;
							while ((curr_h) && (i < gui->hatch_idx)){
								i++;
								curr_h = curr_h->next;
							}
							/* optional rotation and scale */
							rot = gui->patt_ang;
							scale = gui->patt_scale;
						}
						
						/* make DXF HATCH entity */
						dxf_node *new_hatch_el = dxf_new_hatch (curr_h, bound,
						gui->h_type == HATCH_SOLID, gui->hatch_assoc,
						0, 0, /* style, type */
						rot, scale,
						gui->color_idx, /* color, layer */
            (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
						/* line type, line weight */
            (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
            dxf_lw[gui->lw_idx],
						0, DWG_LIFE); /* paper space */
						
						if (new_hatch_el){
							/* parse entity */
							new_hatch_el->obj.graphics = dxf_graph_parse(gui->drawing, new_hatch_el, 0 , 0);
							/* and append to drawing */
							drawing_ent_append(gui->drawing, new_hatch_el);
							/* add to the undo/redo list*/
							do_add_entry(&gui->list_do, _l("HATCH"));
							do_add_item(gui->list_do.current, NULL, new_hatch_el);
						}
						
						gui->step = 0;
					}
					gui->element = NULL;
					gui_next_step(gui);
				}
				if (gui->ev & EV_MOTION){
					dxf_attr_change(new_el, 6,
            (void *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name));
					dxf_attr_change(new_el, 8,
            (void *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name));
					dxf_attr_change_i(new_el, 10, &gui->step_x[gui->step], -1);
					dxf_attr_change_i(new_el, 20, &gui->step_y[gui->step], -1);
					//dxf_attr_change_i(new_el, 42, &gui->bulge, -1);
					dxf_attr_change(new_el, 370, &dxf_lw[gui->lw_idx]);
					dxf_attr_change(new_el, 62, &gui->color_idx);
					
					new_el->obj.graphics = dxf_graph_parse(gui->drawing, new_el, 0 , 1);
				}
			}
		}
		else{
			if (gui->step == 0) {
				/* try to go to next step */
				gui->step = 1;
				gui->free_sel = 0;
			}
			/* verify if elements in selection list */
			if (gui->step == 1 && (!gui->sel_list->next || (gui->ev & EV_ADD))){
				/* if selection list is empty, back to first step */
				gui->step = 0;
				gui->free_sel = 1;
			}
			
			if (gui->step == 0){
				/* in first step, select the elements to proccess*/
				gui->en_distance = 0;
				gui->sel_ent_filter = ~DXF_NONE;
				gui_simple_select(gui);
        /* user cancel operation */
        if (gui->ev & EV_CANCEL){
          gui->element = NULL;
          gui_default_modal(gui);
          gui->step = 0;
        }
			}
			else if (gui->step == 1){
				
				if (gui->ev & EV_ENTER){
					struct h_pattern *curr_h;// = &(gui->list_pattern);
					struct h_family *curr_fam = gui->hatch_fam.next;
					int i = 0;
					
					double rot = 0.0, scale = 1.0;
					
					if(gui->h_type == HATCH_USER) { /* user definied simple pattern */
						strncpy(gui->list_pattern.name, "USER_DEF", DXF_MAX_CHARS);
						curr_h = &(gui->list_pattern);
						rot = 0.0;
						scale = 1.0;
					}
					else if(gui->h_type == HATCH_SOLID) { /* solid pattern */
						strncpy(gui->list_pattern.name, "SOLID", DXF_MAX_CHARS);
						curr_h = &(gui->list_pattern);
						rot = 0.0;
						scale = 1.0;
					}
					else{ /* pattern from library */
						
						/* get current family */
						curr_h = NULL;
						i = 0;
						while (curr_fam){
							if (gui->hatch_fam_idx == i){
								curr_h = curr_fam->list->next;
								break;
							}
							
							i++;
							curr_fam = curr_fam->next;
						}
						
						/* get current hatch pattern */
						i = 0;
						while ((curr_h) && (i < gui->hatch_idx)){
							i++;
							curr_h = curr_h->next;
						}
						/* optional rotation and scale */
						rot = gui->patt_ang;
						scale = gui->patt_scale;
					}
					
					/* make DXF HATCH entity */
					dxf_node *new_hatch_el = dxf_new_hatch2 (curr_h, gui->sel_list,
					gui->h_type == HATCH_SOLID, gui->hatch_assoc,
					gui->hatch_t_box, /* option to make text box boundary */
					0, 0, /* style, type */
					rot, scale,
					gui->color_idx, /* color, layer */
          (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
					/* line type, line weight */
          (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
          dxf_lw[gui->lw_idx],
					0, DWG_LIFE); /* paper space */
					
					if (new_hatch_el){
						/* parse entity */
						new_hatch_el->obj.graphics = dxf_graph_parse(gui->drawing, new_hatch_el, 0 , 0);
						/* and append to drawing */
						drawing_ent_append(gui->drawing, new_hatch_el);
						/* add to the undo/redo list*/
						do_add_entry(&gui->list_do, _l("HATCH"));
						do_add_item(gui->list_do.current, NULL, new_hatch_el);
					}
				}
				else if (gui->ev & EV_CANCEL){
					gui_default_modal(gui);
				}
			}
		}
	}
	return 1;
}

int gui_hatch_info (gui_obj *gui){
	if (gui->modal == HATCH) {
		struct h_pattern *curr_h = NULL;
		struct h_family *curr_fam = gui->hatch_fam.next;
		
		int i = 0;
		
		nk_layout_row_dynamic(gui->ctx, 20, 1);
		nk_label(gui->ctx, _l("Place a Hatch:"), NK_TEXT_LEFT);
		
		/* Selection mode option - Points or Selection */
		nk_style_push_vec2(gui->ctx, &gui->ctx->style.window.spacing, nk_vec2(0,0));
		nk_layout_row_begin(gui->ctx, NK_STATIC, 20, 3);
		nk_layout_row_push(gui->ctx, 45);
		nk_label(gui->ctx, _l("Mode:"), NK_TEXT_LEFT);
		if (gui_tab (gui, _l("Points"), gui->hatch_sel == 0)) {
			gui->hatch_sel = 0;
			gui->step = 0;
		}
		if (gui_tab (gui, _l("Selection"), gui->hatch_sel == 1)) {
			gui->hatch_sel = 1;
			gui->step = 0;
		}
		nk_style_pop_vec2(gui->ctx);
		nk_layout_row_end(gui->ctx);
		
		nk_layout_row_dynamic(gui->ctx, 5, 1);
		
		nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 2, (float[]){0.65f, 0.35f});
		/* associative flag for Hatch*/
		nk_checkbox_label(gui->ctx, _l("Associative"), &gui->hatch_assoc);
		/* in selection mode, option to include text box as hacth boundary*/
		if (gui->hatch_sel) nk_checkbox_label(gui->ctx, _l("Text"), &gui->hatch_t_box);
		
		nk_layout_row_dynamic(gui->ctx, 5, 1);
		
		/* Tabs for select three options:
			- User definied simple hatch;
			- Hatch pattern from a library;
			- Solid fill; */
		nk_style_push_vec2(gui->ctx, &gui->ctx->style.window.spacing, nk_vec2(0,0));
		nk_layout_row_begin(gui->ctx, NK_STATIC, 20, 4);
		if (gui_tab (gui, _l("User"), gui->h_type == HATCH_USER)) gui->h_type = HATCH_USER;
		if (gui_tab (gui, _l("Library"), gui->h_type == HATCH_PREDEF)) gui->h_type = HATCH_PREDEF;
		if (gui_tab (gui, _l("Solid"), gui->h_type == HATCH_SOLID)) gui->h_type = HATCH_SOLID;
		nk_style_pop_vec2(gui->ctx);
		nk_layout_row_end(gui->ctx);
		
		nk_layout_row_dynamic(gui->ctx, 105, 1);
		if (nk_group_begin(gui->ctx, "Patt_controls", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
		
			if (gui->h_type == HATCH_USER){/*User definied simple hatch*/
				/* the user can select only angle and spacing of continuous lines*/
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				gui->user_patt.ang = nk_propertyd(gui->ctx, _l("Angle"), 0.0, gui->user_patt.ang, 360.0, 0.1, 0.1f);
				gui->user_patt.dy = nk_propertyd(gui->ctx, _l("Spacing"), 0.0, gui->user_patt.dy, 1e9, SMART_STEP(gui->user_patt.dy), SMART_STEP(gui->user_patt.dy));
			}
			else if (gui->h_type == HATCH_PREDEF){ /*Hatch pattern from a library */
				/*the library or family of pattern hatchs is a .pat file, according the
				Autodesk especification.
				
				The Standard family is embeded in program and the user can load
				other librarys from files (see the popup windows below)*/
				
				/* get current family */
				curr_fam = gui->hatch_fam.next;
				i = 0;
				gui->patt_name[0] = 0; /*clear data of current pattern */
				gui->patt_descr[0] = 0;
				curr_h = NULL;
				while (curr_fam){
					if (gui->hatch_fam_idx == i){
						strncpy(gui->h_fam_name, curr_fam->name, DXF_MAX_CHARS);
						strncpy(gui->h_fam_descr, curr_fam->descr, DXF_MAX_CHARS);
						curr_h = curr_fam->list->next;
					}
					
					i++;
					curr_fam = curr_fam->next;
				}
				
				/*get current pattern */
				i = 0;
				while (curr_h){
					if (gui->hatch_idx == i){
						strncpy(gui->patt_name, curr_h->name, DXF_MAX_CHARS);
						strncpy(gui->patt_descr, curr_h->descr, DXF_MAX_CHARS);
					}
					
					i++;
					curr_h = curr_h->next;
				}
				
				/* for selection of pattern*/
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				if (nk_button_label(gui->ctx, _l("Explore"))) gui->show_hatch_mng = 1;//show_pat_pp = 1;
				
				/*show data of current pattern*/
				nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 2, (float[]){0.2f, 0.8f});
				nk_label(gui->ctx, _l("Name:"), NK_TEXT_RIGHT);
				nk_label_colored(gui->ctx, gui->patt_name, NK_TEXT_CENTERED, nk_rgb(255,255,0));
				
				/* optional rotation and scale */
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				gui->patt_scale = nk_propertyd(gui->ctx, _l("#Scale"), 1e-9, gui->patt_scale, 1e9, SMART_STEP(gui->patt_scale), SMART_STEP(gui->patt_scale));
				gui->patt_ang = nk_propertyd(gui->ctx, _l("Angle"), 0.0, gui->patt_ang, 360.0, 0.1, 0.1f);
			}
			nk_group_end(gui->ctx);
		}
		
		nk_layout_row_dynamic(gui->ctx, 5, 1);
		/*messages for user in iteractive mode*/
		nk_layout_row_dynamic(gui->ctx, 20, 1);
		if (!gui->hatch_sel){
			if (gui->step == 0){
				nk_label(gui->ctx, _l("Enter first point"), NK_TEXT_LEFT);
			} else {
				nk_label(gui->ctx, _l("Enter next point"), NK_TEXT_LEFT);
			}
		}
		else{
			if (gui->step == 0){
				nk_label(gui->ctx, _l("Select/Add element"), NK_TEXT_LEFT);
			}
			else	nk_label(gui->ctx, _l("Confirm"), NK_TEXT_LEFT);
		}
	}
	return 1;
}

int gui_hatch_mng (gui_obj *gui){
	int show = 1;
	int i = 0;
	static char patt_name[DXF_MAX_CHARS], patt_descr[DXF_MAX_CHARS];
	
	static int show_pat_file = 0;
	static int patt_idx = 0, last_idx = -1;
	struct h_pattern *curr_h = NULL;
	struct h_family *curr_fam = gui->hatch_fam.next;
	static double patt_scale = 1, patt_rot = 0.0;
	
	static struct nk_rect s = {215, 95, 420, 490};
	
	
	if (nk_begin(gui->ctx, _l("Hatch Pattern"), s,
	NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
	NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE)){
		graph_obj *ref_graph = NULL, *curr_graph = NULL;
		list_node * pat_g = NULL;
		
		
		int pat_ei = 0; /*extents flag */
		/* extents and zoom parameters */
		double pat_x0, pat_y0, pat_x1, pat_y1, z, z_x, z_y, o_x, o_y;
		double pat_z0, pat_z1;
		double cosine, sine, dx, dy, max;
		double ang, ox, oy, dash[20];
		int num_dash;
		
		/* get current family */
		curr_fam = gui->hatch_fam.next;
		i = 0;
		gui->patt_name[0] = 0; /*clear data of current pattern */
		gui->patt_descr[0] = 0;
		curr_h = NULL;
		while (curr_fam){
			if (gui->hatch_fam_idx == i){
				strncpy(gui->h_fam_name, curr_fam->name, DXF_MAX_CHARS);
				strncpy(gui->h_fam_descr, curr_fam->descr, DXF_MAX_CHARS);
				curr_h = curr_fam->list->next;
			}
			
			i++;
			curr_fam = curr_fam->next;
		}
		int num_fam = i;
		
		nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 3, (float[]){0.15, 0.75, 0.1});
		nk_label(gui->ctx, _l("Family:"), NK_TEXT_RIGHT);
		
		int h = num_fam * 25 + 5;
		h = (h < 300)? h : 300;
		/* selection of families */
		if (nk_combo_begin_label(gui->ctx, gui->h_fam_name, nk_vec2(270, h))){
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			curr_fam = gui->hatch_fam.next;
			i = 0;
			while (curr_fam){
				if (nk_button_label(gui->ctx, curr_fam->name)){
					gui->hatch_fam_idx = i;
					gui->hatch_idx = 0;
					nk_combo_close(gui->ctx);
					patt_idx = 0;
					last_idx = -1;
				}
				i++;
				curr_fam = curr_fam->next;
			}
			
			nk_combo_end(gui->ctx);
		}
		/* for load other pattern families from file */
		if (nk_button_symbol(gui->ctx, NK_SYMBOL_PLUS)) show_pat_file = 1;
		
		
		/* get current family */
		curr_fam = gui->hatch_fam.next;
		i = 0;
		curr_h = NULL;
		while (curr_fam){
			if (gui->hatch_fam_idx == i){
				curr_h = curr_fam->list->next;
			}
			
			i++;
			curr_fam = curr_fam->next;
		}
		
		/* show data of current family */
		nk_layout_row_dynamic(gui->ctx, 50, 1);
		nk_label_colored_wrap(gui->ctx, gui->h_fam_descr, nk_rgb(100,115,255));
		
		/* show and allow selection of patterns in current library*/
		nk_layout_row_dynamic(gui->ctx, 360, 2);
		if (nk_group_begin(gui->ctx, "Patt_names", NK_WINDOW_BORDER)) {
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			i = 0;
			while (curr_h){
				if (nk_button_label(gui->ctx, curr_h->name)){
					patt_idx = i;
				}
				i++;
				curr_h = curr_h->next;
			}
			nk_group_end(gui->ctx);
		}
		
		/* in next, create the preview visualization of selected pattern*/
		/* get current family */
		curr_fam = gui->hatch_fam.next;
		i = 0;
		curr_h = NULL;
		while (curr_fam){
			if (gui->hatch_fam_idx == i){
				curr_h = curr_fam->list->next;
			}
			i++;
			curr_fam = curr_fam->next;
		}
		
		/* get selected hatch pattern */
		i = 0;
		while (curr_h){
			strncpy(patt_name, curr_h->name, DXF_MAX_CHARS);
			strncpy(patt_descr, curr_h->descr, DXF_MAX_CHARS);
			if (patt_idx == i) break;
			
			i++;
			curr_h = curr_h->next;
		}
		
		/* calcule the ideal (almost) scale for preview */
		struct hatch_line *curr_l = NULL;
		if (curr_h){
			max = 0.0;
			double spc = 0.0;
			
			if (patt_idx != last_idx){
				curr_l = curr_h->lines;
				while (curr_l){
					spc = sqrt(curr_l->dx *curr_l->dx + curr_l->dy * curr_l->dy);
					if (curr_l->num_dash < 2)
						max = (max > spc) ? max : spc;
					else
						max = (max > spc / curr_l->num_dash) ? max : spc / curr_l->num_dash;
				
					curr_l = curr_l->next;
				}
				
				if (max > 0.0) patt_scale = 1/max;
				else patt_scale = 1.0;
				
				//if (curr_h->num_lines > 1) patt_scale *= sqrt(curr_h->num_lines);
				if (curr_h->num_lines > 1) patt_scale *= 1.0 + (double) curr_h->num_lines / 10.0;
				
				patt_rot = 0.0;
				
				last_idx = patt_idx;
			}
			
			pat_g = list_new(NULL, FRAME_LIFE);
			
			/*create reference graph bondary (10 x 10 units) */
			ref_graph = graph_new(FRAME_LIFE);
			line_add(ref_graph, 0.0, 0.0, 0.0, 10.0, 0.0, 0.0);
			line_add(ref_graph, 10.0, 0.0, 0.0, 10.0, 10.0, 0.0);
			line_add(ref_graph, 10.0, 10.0, 0.0, 0.0, 10.0, 0.0);
			line_add(ref_graph, 0.0, 10.0, 0.0, 0.0, 0.0, 0.0);
			
			curr_l = curr_h->lines;
		}
		
		/* create graph for each definition lines in pattern */
		while (curr_l){
			/* apply scale and rotation*/
			ang = fmod(curr_l->ang + patt_rot, 360.0);
			cosine = cos(ang * M_PI/180);
			sine = sin(ang * M_PI/180);
			dx = patt_scale * (cosine*curr_l->dx - sine*curr_l->dy);
			dy = patt_scale * (sine*curr_l->dx + cosine*curr_l->dy);
			cosine = cos(patt_rot * M_PI/180);
			sine = sin(patt_rot * M_PI/180);
			ox = patt_scale * (cosine*curr_l->ox - sine*curr_l->oy);
			oy = patt_scale * (sine*curr_l->ox + cosine*curr_l->oy);
			num_dash = curr_l->num_dash;
			for (i = 0; i < num_dash; i++){
				dash[i] = patt_scale * curr_l->dash[i];
			}
			if (num_dash == 0) { /* for continuous line*/
				dash[0] = 1.0;
				num_dash = 1;
			}
			/* get hatch graph of current def line*/
			curr_graph = graph_hatch(ref_graph, ang * M_PI/180,
				ox, oy,
				dx, dy,
				dash, num_dash,
				FRAME_LIFE);
			
			if ((curr_graph != NULL) && (pat_g != NULL)){
				/*change color -> white*/
				curr_graph->color.r = 255;// - gui->preview[PRV_HATCH]->bkg.r;
				curr_graph->color.g = 255;// - gui->preview[PRV_HATCH]->bkg.g;
				curr_graph->color.b = 255;// - gui->preview[PRV_HATCH]->bkg.b;
				
				list_push(pat_g, list_new((void *)curr_graph, FRAME_LIFE));
			}
			
			curr_l = curr_l->next;
		}
		
		/* calcule the zoom and offset for preview */
		graph_list_ext(pat_g, &pat_ei, &pat_x0, &pat_y0, &pat_z0, &pat_x1, &pat_y1, &pat_z1);
		
		z_x = fabs(pat_x1 - pat_x0)/gui->preview[PRV_HATCH]->width;
		z_y = fabs(pat_y1 - pat_y0)/gui->preview[PRV_HATCH]->height;
		z = (z_x > z_y) ? z_x : z_y;
		if (z <= 0) z =1;
		else z = 1/(1.1 * z);
		o_x = pat_x0 - (fabs((pat_x1 - pat_x0)*z - gui->preview[PRV_HATCH]->width)/2)/z;
		o_y = pat_y0 - (fabs((pat_y1 - pat_y0)*z - gui->preview[PRV_HATCH]->height)/2)/z;
		
		/* draw graphics in preview bitmap */
		bmp_fill(gui->preview[PRV_HATCH], gui->preview[PRV_HATCH]->bkg); /* clear bitmap */
		//graph_list_draw(pat_g, gui->preview[PRV_HATCH], o_x, o_y, z);
		struct draw_param d_param;
		
		d_param.ofs_x = o_x;
		d_param.ofs_y = o_y;
		d_param.ofs_z = 0;
		d_param.scale = z;
		d_param.list = NULL;
		d_param.subst = NULL;
		d_param.len_subst = 0;
		d_param.inc_thick = 0;
		graph_list_draw(pat_g, gui->preview[PRV_HATCH], d_param);
		
		
		
		if (nk_group_begin(gui->ctx, "Patt_prev", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
			/*show data of current pattern*/
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label_colored(gui->ctx, patt_name, NK_TEXT_CENTERED, nk_rgb(255,255,0));
			
			/* preview img */
			nk_layout_row_dynamic(gui->ctx, 175, 1);
			nk_button_image(gui->ctx,  nk_image_ptr(gui->preview[PRV_HATCH]));
			nk_layout_row_dynamic(gui->ctx, 50, 1);
			nk_label_colored_wrap(gui->ctx, patt_descr, nk_rgb(100,115,255));
			
			nk_layout_row_dynamic(gui->ctx, 20, 1);
			nk_label(gui->ctx, _l("Ref: 10 x 10 units"), NK_TEXT_CENTERED);
			/* optional parameters -> change the preview */
			patt_scale = nk_propertyd(gui->ctx, _l("#Scale"), 1e-9, patt_scale, 1e9, SMART_STEP(patt_scale), SMART_STEP(patt_scale));
			patt_rot = nk_propertyd(gui->ctx, _l("#Rotation"), 0.00, patt_rot, 360.0, 0.1, 0.1);
			
			if (nk_button_label(gui->ctx, _l("Choose"))){ /*done the selection*/
				/* update  the main parameters */
				gui->hatch_idx = patt_idx;
				gui->patt_scale = patt_scale;
				gui->patt_ang = patt_rot;
				show = 0; /* close */
			}
			nk_group_end(gui->ctx);
		}
		
		
		if (show_pat_file){
			/* Load other pattern libraries */
			static char pat_path[PATH_MAX_CHARS+1];
			
			/* supported file format */
			static const char *ext_type[] = {
				"PAT",
				"*"
			};
      static char ext_descr[2][DXF_MAX_CHARS + 1];
      strncpy(ext_descr[0], _l("Hatch Pattern Library (.pat)"), DXF_MAX_CHARS);
      strncpy(ext_descr[1], _l("All files (*)"), DXF_MAX_CHARS);
			
			#define FILTER_COUNT 2
			
			if (gui->show_file_br == 2){
				/* update string with returned path from browser window */
				strncpy (pat_path, gui->curr_path , PATH_MAX_CHARS);
				gui->show_file_br = 0;
			}
			
			static struct nk_rect s = {20, -20, 300, 120};
			if (nk_popup_begin(gui->ctx, NK_POPUP_STATIC, _l("Add pattern family"), NK_WINDOW_CLOSABLE, s)){
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				nk_label(gui->ctx, _l("File to Open:"), NK_TEXT_CENTERED);
				/* get the file path */
				nk_edit_focus(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, pat_path, PATH_MAX_CHARS, nk_filter_default);
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("OK"))) {
					/* check if filename extension is ".pat" */
					char *ext = get_ext(pat_path);
					if (strcmp(ext, "pat") == 0){
						/*use the filename without extension for name the library */
						char *filename = get_filename(pat_path);
						strip_ext(filename);
						/* parse the file*/
						gui->end_fam->next = dxf_hatch_family_file(filename, pat_path);
						/* and append to list*/
						if(gui->end_fam->next) gui->end_fam = gui->end_fam->next;
						show_pat_file = nk_false; /*close the window*/
					}
					
				}
				if (nk_button_label(gui->ctx, _l("Explore"))) {
					for (i = 0; i < FILTER_COUNT; i++){
						gui->file_filter_types[i] = ext_type[i];
						gui->file_filter_descr[i] = ext_descr[i];
					}
					gui->file_filter_count = FILTER_COUNT;
					gui->filter_idx = 0;
					
					gui->show_file_br = 1;
					gui->curr_path[0] = 0;
				}
				nk_popup_end(gui->ctx);
			} else {
				show_pat_file = nk_false;
			}
		}
	} else show = 0;
	nk_end(gui->ctx);
	
	return show;
}