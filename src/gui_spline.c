#include "gui_use.h"

int gui_spline_interactive(gui_obj *gui){
	if (gui->modal == SPLINE){
		static dxf_node *new_el = NULL;
		dxf_node *spline = NULL;
		
		if (gui->step == 0){
			gui->free_sel = 0;
			if (gui->ev & EV_ENTER){
				dxf_mem_pool(ZERO_DXF, ONE_TIME);
				
				/* create a new DXF spline */
				new_el = (dxf_node *) dxf_new_lwpolyline (
					gui->step_x[gui->step], gui->step_y[gui->step], 0.0, /* pt1, */
					0.0, /* bulge */
					7, "0", /* color, layer */
					"Continuous", 0, /* line type, line weight */
					0, ONE_TIME); /* paper space */
				dxf_lwpoly_append (new_el, gui->step_x[gui->step], gui->step_y[gui->step], 0.0, 0.0, ONE_TIME);
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
				
				dxf_lwpoly_append (new_el, gui->step_x[gui->step], gui->step_y[gui->step], 0.0, 0.0, ONE_TIME);
				gui->step = 2;
				gui_next_step(gui);
			}
			else if (gui->ev & EV_CANCEL){
				gui->draw_tmp = 0;
				gui->draw_phanton = 0;
				gui->phanton = NULL;
				
				if (gui->step == 2){
					dxf_lwpoly_remove (new_el, -1);
					
					if (gui->spline_mode == SP_CTRL){					
						spline =  dxf_new_spline (new_el, gui->sp_degree - 1, gui->closed,
							gui->color_idx, /* color, layer */
              (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
							/* line type, line weight */
              (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
              dxf_lw[gui->lw_idx],
							0, DWG_LIFE); /* paper space */
					}
					else {
						spline =  dxf_new_spline2 (new_el, gui->closed,
							gui->color_idx, /* color, layer */
              (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
							/* line type, line weight */
              (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
              dxf_lw[gui->lw_idx],
							0, DWG_LIFE); /* paper space */
					}
					
					if (spline){
						spline->obj.graphics = dxf_graph_parse(gui->drawing, spline, 0 , DWG_LIFE);
						drawing_ent_append(gui->drawing, spline);
						
						do_add_entry(&gui->list_do, _l("SPLINE"));
						do_add_item(gui->list_do.current, NULL, spline);
					}
					gui->step = 0;
				}
        new_el = NULL;
				gui->element = NULL;
				gui_first_step(gui);
			}
			if (gui->ev & EV_MOTION){
				dxf_attr_change_i(new_el, 10, &gui->step_x[gui->step], -1);
				dxf_attr_change_i(new_el, 20, &gui->step_y[gui->step], -1);
			}
			
			gui->draw_phanton = 1;
			if (gui->spline_mode == SP_CTRL){
				spline =  dxf_new_spline (new_el, gui->sp_degree - 1, gui->closed,
					gui->color_idx, /* color, layer */
          (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
					/* line type, line weight */
          (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
          dxf_lw[gui->lw_idx],
					0, FRAME_LIFE); /* paper space */
			}
			else {
				spline =  dxf_new_spline2 (new_el, gui->closed,
					gui->color_idx, /* color, layer */
          (char *) strpool_cstr2( &name_pool, gui->drawing->layers[gui->layer_idx].name),
					/* line type, line weight */
          (char *) strpool_cstr2( &name_pool, gui->drawing->ltypes[gui->ltypes_idx].name),
          dxf_lw[gui->lw_idx],
					0, FRAME_LIFE); /* paper space */
			}
			if (spline)
				gui->phanton = dxf_graph_parse(gui->drawing, spline, 0 , FRAME_LIFE);
			else
				gui->phanton = NULL;
			
			graph_obj *graph = dxf_lwpline_parse(gui->drawing, new_el, 0 , FRAME_LIFE);
			
			if (graph){
				graph->patt_size = 2;
				graph->pattern[0] = 10 / gui->zoom;
				graph->pattern[1] = -10 / gui->zoom;
				
				if(!gui->phanton)
					gui->phanton = list_new(NULL, FRAME_LIFE);
				
				list_node * new_node = list_new(graph, FRAME_LIFE);
				list_push(gui->phanton, new_node);
			}
		}
	}
	return 1;
}

int gui_spline_info (gui_obj *gui){
	if (gui->modal == SPLINE) {
    static char mode[2][DXF_MAX_CHARS + 1];
    strncpy(mode[0], _l("Control points"), DXF_MAX_CHARS);
    strncpy(mode[1], _l("Fit points"), DXF_MAX_CHARS);
    
    char *mode_addr[] = {mode[0], mode[1]};
		
		nk_layout_row_dynamic(gui->ctx, 20, 1);
		nk_label(gui->ctx, _l("Place a spline by:"), NK_TEXT_LEFT);
		
		gui->spline_mode = nk_combo(gui->ctx, (const char **)mode_addr, 2, gui->spline_mode, 20, nk_vec2(150, 55));
		if (gui->spline_mode == SP_CTRL)
			nk_property_int(gui->ctx, _l("Degree"), 2, &gui->sp_degree, 15, 1, 0.1);
		nk_checkbox_label(gui->ctx, _l("Closed"), &gui->closed);
		
		
		if (gui->step == 0){
			nk_label(gui->ctx, _l("Enter first point"), NK_TEXT_LEFT);
		} else {
			nk_label(gui->ctx, _l("Enter next point"), NK_TEXT_LEFT);
		}
	}
	return 1;
}