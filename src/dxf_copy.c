#include "dxf_copy.h"
#include "dxf_create.h"

dxf_node *dxf_ent_copy(dxf_node *source, int pool_dest){
	dxf_node *current = NULL;
	dxf_node *prev = NULL, *dest = NULL, *curr_dest = NULL, *new_ent = NULL;
	
	if (source){ 
		if (source->type == DXF_ENT){
			if (source->obj.content){
				current = source->obj.content->next;
				prev = current;
				dest = dxf_obj_new2 ( source->obj.id, pool_dest );
				curr_dest = dest;
			}
		}
	}

	while ((current) && (curr_dest)){
		if (current->type == DXF_ENT){
			
			if (current->obj.content){
				new_ent = dxf_obj_new2 ( current->obj.id, pool_dest );
				dxf_obj_append(curr_dest, new_ent);
				curr_dest = new_ent;
				
				/* starts the content sweep */
				current = current->obj.content->next;
				prev = current;
				
				continue;
			}
		}
		else if (current->type == DXF_ATTR){ /* DXF attribute */
      dxf_attr_append_cpy(curr_dest, current, pool_dest);
		}
		
		current = current->next; /* go to the next in the list */
		/* ============================================================= */
		while (current == NULL){
			/* end of list sweeping */
			if ((prev == NULL) || (prev == source)){ /* stop the search if back on initial entity */
				current = NULL;
				break;
			}
			/* try to back in structure hierarchy */
			prev = prev->master;
			curr_dest = curr_dest->master;
			if (prev){ /* up in structure */
				/* try to continue on previous point in structure */
				current = prev->next;
				if(prev == source){
					current = NULL;
					break;
				}
				
			}
			else{ /* stop the search if structure ends */
				current = NULL;
				break;
			}
		}
	}
	
	return dest;
}

dxf_node *dxf_ent_cpy_simple(dxf_node *source, int pool_dest){
	dxf_node *current = NULL;
	dxf_node *prev = NULL, *dest = NULL, *curr_dest = NULL, *new_ent = NULL;
	
	if (source){ 
		if (source->type == DXF_ENT){
			if (source->obj.content){
				current = source->obj.content->next;
				prev = current;
				
        dest = dxf_obj_new2 ( source->obj.id, pool_dest );
				curr_dest = dest;
			}
		}
	}

	while ((current) && (curr_dest)){
		if (current->type == DXF_ATTR){ /* DXF attibute */
      dxf_attr_append_cpy(curr_dest, current, pool_dest);
			
		}
		
		current = current->next; /* go to the next in the list */
		
	}
	
	return dest;
}

dxf_node * dxf_drwg_cpy(dxf_drawing *source, dxf_drawing *dest, dxf_node *obj){
	
	if (obj->type != DXF_ENT) return NULL;
  dxf_node *new_ent = NULL;
  int typ = dxf_ident_ent_type(obj);
	
  if (typ == DXF_BLK) return NULL;
  if (typ == DXF_ENDBLK) return NULL;
	
  if (typ == DXF_INSERT || typ == DXF_DIMENSION){
		dxf_node *block = NULL, *blk_name = NULL;
		blk_name = dxf_find_attr2(obj, 2);
		if(!blk_name) return NULL;
		
		block = dxf_find_obj_descr2(source->blks, "BLOCK",
      (char*)strpool_cstr2( &name_pool, blk_name->value.str));
		if(!block) return NULL;
		dxf_node *block_rec = NULL, *new_block = NULL;
		if (!dxf_block_cpy(source, dest, block, &block_rec, &new_block)) return NULL;
	}
  if ( typ == DXF_DIMENSION ){
		/* copy DIMSTYLE */
		dxf_node *dim_sty = NULL, *dim_sty_nam = NULL;
		dim_sty_nam = dxf_find_attr2(obj, 3);
		if(!dim_sty_nam) return NULL;
    /* find name in 'value' pool */
    char * d_sty_nm = (char*)strpool_cstr2( &value_pool, dim_sty_nam->value.str);
		
		dim_sty = dxf_find_obj_descr2(source->t_dimst, "DIMSTYLE", d_sty_nm);
		if(!dim_sty) return NULL;
		
		/* verify if DIMSTYLE not exist */
		if (!dxf_find_obj_descr2(dest->t_dimst, "DIMSTYLE", d_sty_nm)){
		
			dim_sty = dxf_ent_copy(dim_sty, dest->pool);
			if(!dim_sty) return NULL;
			
			int ok = ent_handle(dest, dim_sty);
			if (ok) ok = dxf_obj_append(dest->t_dimst, dim_sty);
			if (!ok) return NULL;
		}
	}
	
  if ( typ == DXF_IMAGE ){
		/*copy IMAGEDEF */
		
		dxf_node *imgdef_obj = dxf_find_attr2(obj, 340);
		if(imgdef_obj){
			long imgdef_id = strtol(strpool_cstr2( &value_pool, imgdef_obj->value.str) , NULL, 16);
			/* try to find original IMAGEDEF object */
			dxf_node *imgdef = dxf_find_handle(source->objs, imgdef_id);
			if (imgdef) {
				/* copy IMAGEDEF obj to destination drawing */

				dxf_node *path = dxf_find_attr2(imgdef, 1);
				if (!path) return NULL;
				imgdef = dxf_new_imgdef (
          (char*)strpool_cstr2( &value_pool, path->value.str), dest->pool);
				dxf_node *handle = NULL;
				if (ent_handle(dest, imgdef))
					handle = dxf_find_attr2(imgdef, 5);
				else return NULL; /* Fail */
				if (handle) {
					if(!dxf_obj_append(dest->objs, imgdef)) return NULL; /* Fail */
				}
				else return NULL; /* Fail */
				
				if (!(new_ent = dxf_ent_copy(obj, dest->pool))) return NULL;
				if (!(imgdef_obj = dxf_find_attr2(new_ent, 340))) return NULL;
				
				/* update cross reference */
        imgdef_obj->value.str = handle->value.str;
			}
			else return NULL; /* Fail */
		}
		else return NULL; /* Fail */
		
		/* remove link to IMAGEDEF_REACTOR, if exists */
		imgdef_obj = dxf_find_attr2(new_ent, 360);
		if(imgdef_obj)
			dxf_obj_subst(imgdef_obj, NULL);
		
	}
	else new_ent = dxf_ent_copy(obj, dest->pool);
	
	
	return new_ent;
}

int dxf_drwg_ent_cpy(dxf_drawing *source, dxf_drawing *dest, list_node *list){
	if (dest == NULL || list == NULL) return 0;
	list_node *current = list->next;
	dxf_node *obj = NULL, *new_ent = NULL;
	
	while (current != NULL){
		if (current->data){
			obj = (dxf_node *)current->data;
			if (obj->type == DXF_ENT){ /* DXF entity  */
				new_ent = dxf_drwg_cpy(source, dest, obj);
				drawing_ent_append(dest, new_ent);
				
			}
		}
		current = current->next;
	}
	return 1;

}

list_node * dxf_drwg_ent_cpy_all(dxf_drawing *source, dxf_drawing *dest, int pool_idx){
	if (dest == NULL || source == NULL) return NULL;
	list_node * list_ret = list_new(NULL, pool_idx);
	if (!list_ret) return NULL;
	dxf_node *current = NULL, *new_ent = NULL;
		
	if ((source->ents != NULL) && (source->main_struct != NULL)){
		current = source->ents->obj.content->next;
		
		while (current != NULL){
			if (current->type == DXF_ENT){ // DXF entity
				new_ent = dxf_drwg_cpy(source, dest, current);
				drawing_ent_append(dest, new_ent);
				
				if (new_ent) list_push(list_ret, list_new((void *)new_ent, pool_idx));
			}
			current = current->next;
		}
	}
	return list_ret;
}

int dxf_cpy_layer (dxf_drawing *drawing, dxf_node *layer){
	
	if (!drawing) 
		return 0; /* error -  not drawing */
	
	if ((drawing->t_layer == NULL) || (drawing->main_struct == NULL)) 
		return 0; /* error -  not main structure */
	
	if (!layer) 
		return 0; /* error -  not layer */
	
  STRPOOL_U64 name = 0, ltype = 0;
	int color = 0, line_w = 0, flags = 0;
	dxf_node *current = NULL;
		
	/* and sweep its content */
	if (layer->obj.content) current = layer->obj.content->next;
	while (current){
		if (current->type == DXF_ATTR){
			switch (current->value.group){
				case 2: /* layer name */
          name = current->value.str;
					break;
				case 6: /* layer line type name */
          ltype = current->value.str;
					break;
				case 62: /* layer color */
					color = current->value.i_data;
					if (color < 0) {
						color = abs(color);
					}
					break;
				case 70: /* flags */
					flags = current->value.i_data;
					break;
				case 370:
					line_w = current->value.i_data;
			}
		}
		current = current->next;
	}
  
  if (!name)  return 0; /* error -  no name */
	
	/* verify if not exists */
  if (dxf_find_obj_descr2(drawing->t_layer, "LAYER", 
    (char*)strpool_cstr2( &name_pool, name)) != NULL)
      return 0; /* error -  exists layer with same name */
	
	if ((abs(color) > 255) || (color == 0)) color = 7;
	
	const char *handle = "0";
	const char *plotstylename = "2B"; /* -->TRICK  -  see the handle in seed file */
	const char *dxf_class = "AcDbSymbolTableRecord";
	const char *dxf_subclass = "AcDbLayerTableRecord";
	int int_zero = 0, ok = 0;
	
	/* create a new LAYER */
	dxf_node * lay = dxf_obj_new ("LAYER", drawing->pool);
	
	if (lay) {
		ok = 1;
		ok &= dxf_attr_append(lay, 5, (void *) handle, drawing->pool);
		ok &= dxf_attr_append(lay, 100, (void *) dxf_class, drawing->pool);
		ok &= dxf_attr_append(lay, 100, (void *) dxf_subclass, drawing->pool);
		ok &= dxf_attr_append(lay, 2, (void *) strpool_cstr2( &name_pool, name), drawing->pool);
		ok &= dxf_attr_append(lay, 70, (void *) &int_zero, drawing->pool);
		ok &= dxf_attr_append(lay, 62, (void *) &color, drawing->pool);
		if (ltype)
      ok &= dxf_attr_append(lay, 6, (void *) strpool_cstr2( &name_pool, ltype), drawing->pool);
		ok &= dxf_attr_append(lay, 370, (void *) &line_w, drawing->pool);
		ok &= dxf_attr_append(lay, 390, (void *) plotstylename, drawing->pool);
		
		/* get current handle and increment the handle seed*/
		ok &= ent_handle(drawing, lay);
		
		/* append the layer to correpondent table */
		dxf_append(drawing->t_layer, lay);
		
		/* update the layers in drawing  */
		dxf_layer_assemb (drawing);
	}
	
	return ok;
}

int dxf_cpy_lay_drwg(dxf_drawing *source, dxf_drawing *dest){
	/* copy layer betweew drawings, only in use */
	int ok = 0, i, idx;
	dxf_node *current, *prev, *obj = NULL, *list[2], *lay_obj, *lay_name;
	
	list[0] = NULL; list[1] = NULL;
	if (source){
		list[0] = dest->ents;
		list[1] = dest->blks;
	}
	else return 0;
	
	for (i = 0; i< 2; i++){ /* look in BLOCKS and ENTITIES sections */
		obj = list[i];
		current = obj;
		while (current){ /* sweep elements in section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
				lay_name = dxf_find_attr2(current, 8); /* get element's layer */
				if (lay_name){
          lay_obj = dxf_find_obj_descr2(source->t_layer, "LAYER", 
            (char*)strpool_cstr2( &name_pool, lay_name->value.str));
					dxf_cpy_layer (dest, lay_obj);
					
				}
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/

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
					if(prev == obj){
						current = NULL;
						break;
					}
					
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

dxf_node *dxf_cpy_ltype (dxf_drawing *drawing, dxf_node *ltype){
	
	if (!drawing) 
		return NULL; /* error -  not drawing */
	
	if ((drawing->t_ltype == NULL) || (drawing->main_struct == NULL)) 
		return NULL; /* error -  not main structure */
	
	if (!ltype) 
		return NULL; /* error -  not ltype */
	
  STRPOOL_U64 name = 0, descr = 0;
	int size;
	double length;
	
	dxf_node *current = NULL;
	
	size = 0;
	length = 0;
	
	
	/* and sweep its content */
	if (ltype->obj.content) current = ltype->obj.content->next;
	while (current){
		if (current->type == DXF_ATTR){
			switch (current->value.group){
				case 2: /* ltype name */
          name = current->value.str;
					break;
				case 3: /* ltype descriptive text */
          descr = current->value.str;
					break;
				case 40: /* pattern length */
					length = current->value.d_data;
					break;
				case 73: /* num of pattern elements */
					size = current->value.i_data;
					if (size > DXF_MAX_PAT) {
						size < DXF_MAX_PAT;}
			}
		}
		current = current->next;
	}
	/* error -  no name */
  if (!name) return NULL;
	
	/* verify if not exists */
	if (dxf_find_obj_descr2(drawing->t_ltype, "LTYPE", 
    (char*)strpool_cstr2( &name_pool, name)) != NULL)
      return NULL; /* error -  exists ltype with same name */
	
	const char *handle = "0";
	const char *dxf_class = "AcDbSymbolTableRecord";
	const char *dxf_subclass = "AcDbLinetypeTableRecord";
	int int_zero = 0, ok = 0, align = 65;
	
	/* create a new LTYPE */
	dxf_node * l_typ = dxf_obj_new ("LTYPE", drawing->pool);
	
	if (l_typ) {
		ok = 1;
		ok &= dxf_attr_append(l_typ, 5, (void *) handle, drawing->pool);
		ok &= dxf_attr_append(l_typ, 100, (void *) dxf_class, drawing->pool);
		ok &= dxf_attr_append(l_typ, 100, (void *) dxf_subclass, drawing->pool);
		ok &= dxf_attr_append(l_typ, 2, (void *) strpool_cstr2( &name_pool, name), drawing->pool);
		ok &= dxf_attr_append(l_typ, 70, (void *) &int_zero, drawing->pool);
		//ok &= dxf_attr_append(l_typ, 3, (void *) descr, drawing->pool);
    if (descr)
      ok &= dxf_attr_append(l_typ, 3, (void *) strpool_cstr2( &value_pool, descr), drawing->pool);
		ok &= dxf_attr_append(l_typ, 72, (void *) &align, drawing->pool);
		ok &= dxf_attr_append(l_typ, 73, (void *) &size, drawing->pool);
		ok &= dxf_attr_append(l_typ, 40, (void *) &length, drawing->pool);
		
		if (ltype->obj.content) current = ltype->obj.content->next;
		while (current){
			if (current->type == DXF_ATTR){
				switch (current->value.group){
					case 49: /* pattern element */
						ok &= dxf_attr_append(l_typ, 49, 
							(void *) &current->value.d_data,
							drawing->pool);
						break;
					case 74: /* pattern element flag*/
						ok &= dxf_attr_append(l_typ, 74, 
							(void *) &current->value.i_data,
							drawing->pool);
						break;
					case 75: /* pattern element - shape number*/
						ok &= dxf_attr_append(l_typ, 75, 
							(void *) &current->value.i_data,
							drawing->pool);
						break;
					case 340: /* pattern element - text style pointer */
						ok &= dxf_attr_append(l_typ, 340,
              (void *) strpool_cstr2( &value_pool, current->value.str),
							drawing->pool);
						break;
					case 46: /* pattern element scale */
						ok &= dxf_attr_append(l_typ, 46, 
							(void *) &current->value.d_data,
							drawing->pool);
						break;
					case 50: /* pattern element rotation*/
						ok &= dxf_attr_append(l_typ, 50, 
							(void *) &current->value.d_data,
							drawing->pool);
						break;
					case 44: /* pattern element x offset*/
						ok &= dxf_attr_append(l_typ, 44, 
							(void *) &current->value.d_data,
							drawing->pool);
						break;
					case 45: /* pattern element y offset*/
						ok &= dxf_attr_append(l_typ, 45, 
							(void *) &current->value.d_data,
							drawing->pool);
						break;
					case 9: /* pattern element - text string */
						ok &= dxf_attr_append(l_typ, 9,
              (void *) strpool_cstr2( &value_pool, current->value.str),
							drawing->pool);
						break;
				}
			}
			current = current->next;
		}
		
		/* get current handle and increment the handle seed*/
		ok &= ent_handle(drawing, l_typ);
		
		if (!ok) return NULL;
		
		/* append the ltype to correpondent table */
		dxf_append(drawing->t_ltype, l_typ);
		
		/* update the ltypes in drawing  */
		dxf_ltype_assemb (drawing);
		
		return l_typ;
	}
	
	return NULL;
}

int dxf_cpy_ltyp_drwg(dxf_drawing *source, dxf_drawing *dest){
	/* copy layer betweew drawings, only in use */
	int ok = 0, i, j, idx;
	dxf_node *current, *prev, *obj = NULL, *list[3], *ltyp_obj, *ltyp_name;
	dxf_node *sty_obj = NULL, *style = NULL;
	
	list[0] = NULL; list[1] = NULL; list[2] = NULL;
	if (source){
		list[0] = dest->ents;
		list[1] = dest->blks;
		list[2] = dest->t_layer;
	}
	else return 0;
	
	for (i = 0; i< 3; i++){ /* look in BLOCKS, ENTITIES sections and LAYER table too */
		obj = list[i];
		current = obj;
		
		while (current){ /* sweep elements in section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
				ltyp_name = dxf_find_attr2(current, 6); /* get element's line type */
				if (ltyp_name){
          ltyp_obj = dxf_find_obj_descr2(source->t_ltype, "LTYPE",
            (char*)strpool_cstr2( &name_pool, ltyp_name->value.str));
					ltyp_obj = dxf_cpy_ltype (dest, ltyp_obj);
					
					/* for complex linetype, copy the STYLE object associated */
					long int sty_id = 0;
					for (j = 0; sty_obj = dxf_find_attr_i(ltyp_obj, 340, j); j++){
						sty_id = strtol(
              (char*)strpool_cstr2( &value_pool, sty_obj->value.str), NULL, 16);
						/* try to find original STYLE object */
						style = dxf_find_handle(source->t_style, sty_id);
						if (style) {
							/* copy STYLE obj to destination drawing */
							sty_id = dxf_cpy_style (dest, style);
							/* update pointer in linetype to new obj */
              char hndl_str[DXF_MAX_CHARS+1];
              snprintf(hndl_str, DXF_MAX_CHARS, "%lX", sty_id);
              sty_obj->value.str = strpool_inject( &value_pool, (char const*) hndl_str, strlen(hndl_str) );
						}
					}
				}
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/
			
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
					if(prev == obj){
						current = NULL;
						break;
					}
					
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

long int dxf_cpy_style (dxf_drawing *drawing, dxf_node *style){
	
	if (!drawing) 
		return 0; /* error -  not drawing */
	
	if ((drawing->t_style == NULL) || (drawing->main_struct == NULL)) 
		return 0; /* error -  not main structure */
	
	if (!style) 
		return 0; /* error -  not style */
	
  STRPOOL_U64 name = 0, file_name = 0, big_file = 0;
	
	int flags1;
	int flags2;
	int num_el;
	
	double fixed_h;
	double width_f;
	double oblique;
	dxf_node *current = NULL;
	dxf_node * sty = NULL, *handle_obj = NULL;
		
	flags1 = 0;
	flags2 = 0;
	fixed_h = 0.0;
	width_f = 1.0;
	oblique = 0.0;
	
	
	/* and sweep its content */
	if (style->obj.content) current = style->obj.content->next;
	while (current){
		if (current->type == DXF_ATTR){
			switch (current->value.group){
				case 2: /* tstyle name */
          name = current->value.str;
					break;
				case 3: /* file name */
          file_name = current->value.str;
					break;
				case 4: /* bigfont file name */
          big_file = current->value.str;
					break;
				case 40: /* fixed height*/
					fixed_h = current->value.d_data;
					break;
				case 41: /* width factor*/
					width_f = current->value.d_data;
					break;
				case 50: /* oblique angle*/
					oblique = current->value.d_data;
					break;
				case 70: /* flags */
					flags1 = current->value.i_data;
					break;
				case 71: /* flags */
					flags2 = current->value.i_data;
			}
		}
		current = current->next;
	}
  //if (!name) return 0;
	
	/* verify if not exists */
	if (sty = dxf_find_obj_descr2(drawing->t_style, "STYLE",
      (char *)strpool_cstr2( &name_pool, name))) {
		handle_obj = dxf_find_attr2(sty, 5);
		if (handle_obj) return strtol(
      strpool_cstr2( &value_pool, handle_obj->value.str), NULL, 16);
		return 0; /* error */
	}
	
	const char *handle = "0";
	const char *dxf_class = "AcDbSymbolTableRecord";
	const char *dxf_subclass = "AcDbTextStyleTableRecord";
	long int ok = 0;
	double d_one = 1.0;
	
	/* create a new style */
	sty = dxf_obj_new ("STYLE", drawing->pool);
	
	if (sty) {
		ok = 1;
		ok &= dxf_attr_append(sty, 5, (void *) handle, drawing->pool);
		ok &= dxf_attr_append(sty, 100, (void *) dxf_class, drawing->pool);
		ok &= dxf_attr_append(sty, 100, (void *) dxf_subclass, drawing->pool);
		ok &= dxf_attr_append(sty, 2, (void *) strpool_cstr2( &name_pool, name), drawing->pool);
		ok &= dxf_attr_append(sty, 70, (void *) &flags1, drawing->pool);
		ok &= dxf_attr_append(sty, 40, (void *) &fixed_h, drawing->pool);
		ok &= dxf_attr_append(sty, 41, (void *) &width_f, drawing->pool);
		ok &= dxf_attr_append(sty, 50, (void *) &oblique, drawing->pool);
		ok &= dxf_attr_append(sty, 71, (void *) &flags2, drawing->pool);
		ok &= dxf_attr_append(sty, 42, (void *) &d_one, drawing->pool);
		if (file_name)
      ok &= dxf_attr_append(sty, 3, (void *) strpool_cstr2( &value_pool, file_name), drawing->pool);
		if (big_file)
      ok &= dxf_attr_append(sty, 4, (void *) strpool_cstr2( &value_pool, big_file), drawing->pool);
		
		/* get current handle and increment the handle seed*/
		ok &= ent_handle(drawing, sty);
		if (!ok) return 0; /* error */
		
		/* append the style to correpondent table */
		dxf_append(drawing->t_style, sty);
		
		/* update the styles in drawing  */
		dxf_tstyles_assemb (drawing);
		
		handle_obj = dxf_find_attr2(sty, 5);
		if (handle_obj) return strtol(
      strpool_cstr2( &value_pool, handle_obj->value.str), NULL, 16);
	}
	
	return 0;
}

int dxf_cpy_sty_drwg(dxf_drawing *source, dxf_drawing *dest){
	/* copy style betweew drawings, only in use */
	int ok = 0, i, idx;
	dxf_node *current, *prev, *obj = NULL, *list[4], *sty_obj, *sty_name;
	
	list[0] = NULL; list[1] = NULL; list[2] = NULL; list[3] = NULL;
	if (source){
		list[0] = dest->ents;
		list[1] = dest->blks;
    list[2] = dest->t_ltype;
    list[3] = dest->t_dimst;
	}
	else return 0;
	
	for (i = 0; i < 4; i++){ /* look in BLOCKS and ENTITIES sections, and DIMSTYLE and LTYPE tables*/
		obj = list[i];
		current = obj;
		while (current){ /* sweep elements in section */
			ok = 1;
			prev = current;
			if (current->type == DXF_ENT){
        if (obj == dest->ents || obj == dest->blks){ /* entities */
          sty_name = dxf_find_attr2(current, 7); /* get element's style */
          if (sty_name){
            sty_obj = dxf_find_obj_descr2(source->t_style, "STYLE",
              (char*)strpool_cstr2( &name_pool, sty_name->value.str));
            dxf_cpy_style (dest, sty_obj);
          }
        }
        else if (obj == dest->t_ltype || obj == dest->t_dimst){ /* tables */
          sty_name = dxf_find_attr2(current, 340); /* get element's style handle */
          if (sty_name){
            
            long int id = strtol(strpool_cstr2( &value_pool, sty_name->value.str), NULL, 16);
            /* look for correspondent style object */
            sty_obj = dxf_find_handle(source->t_style, id);
            
            /* copy STYLE obj to destination drawing */
            long int sty_id = dxf_cpy_style (dest, sty_obj);
            /* update pointer in linetype to current obj */
            char hndl_str[DXF_MAX_CHARS+1];
            snprintf(hndl_str, DXF_MAX_CHARS, "%lX", sty_id);
            sty_name->value.str = strpool_inject( &value_pool, (char const*) hndl_str, strlen(hndl_str) );
          }
        }
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
				
			current = current->next; /* go to the next in the list*/

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
					if(prev == obj){
						current = NULL;
						break;
					}
					
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

int dxf_block_cpy(dxf_drawing *source, dxf_drawing *dest, dxf_node *block, dxf_node **block_rec, dxf_node **new_block){
	int ok = 0;
	
  STRPOOL_U64 name = 0, layer = 0;
	if (!dest || !source || !block) return 0;
	dxf_node *blkrec = NULL, *blk = NULL, *endblk = NULL, *handle = NULL;
	dxf_node *obj, *new_ent, *current;
	
	/* get block name */
	obj = dxf_find_attr2(block, 2);
	if(!obj) return 0;
  name = obj->value.str;
	
	/* get block layer */
	obj = dxf_find_attr2(block, 8);
	if(obj) layer = obj->value.str;
	/* verify if block not exist */
	if (dxf_find_obj_descr2(dest->blks, "BLOCK",
    (char*)strpool_cstr2( &name_pool, name))) return 1;
	
	double max_x = 0.0, max_y = 0.0;
	double min_x = 0.0, min_y = 0.0;
	int init_ext = 0;
	
	/* create BLOCK_RECORD table entry*/
	blkrec = dxf_new_blkrec ((char*)strpool_cstr2( &name_pool, name), dest->pool);
	ok = ent_handle(dest, blkrec);
	if (ok) handle = dxf_find_attr2(blkrec, 5); ok = 0;
	
	/* begin block */
	if (handle) blk = dxf_new_begblk (
    (char*)strpool_cstr2( &name_pool, name),
    (char*)strpool_cstr2( &name_pool, layer),
    (char*)strpool_cstr2( &value_pool, handle->value.str), dest->pool);
	/* get a handle */
	ok = ent_handle(dest, blk);
	/* use the handle to owning the ENDBLK ent */
	if (ok) handle = dxf_find_attr2(blk, 5); ok = 0;
	if (handle) endblk = dxf_new_endblk ((char*)strpool_cstr2( &name_pool, layer),
    (char*)strpool_cstr2( &value_pool, handle->value.str), dest->pool);
	
	current = block->obj.content;
	
	while (current){ /* sweep elements in source block */
		if (current->type == DXF_ENT){
			new_ent = dxf_drwg_cpy(source, dest, current);
			ent_handle(dest, new_ent);
			dxf_obj_append(blk, new_ent);
		}
			
		current = current->next; /* go to the next in the list*/
	}
	
	
	/* end the block*/
	if (endblk) ok = ent_handle(dest, endblk);
	if (ok) ok = dxf_obj_append(blk, endblk);
	
	/*attach to blocks section*/
	if (ok) ok = dxf_obj_append(dest->blks_rec, blkrec);
	if (ok) *block_rec = blkrec;
	if (ok) ok = dxf_obj_append(dest->blks, blk);
	if (ok) *new_block = blk;
	
	return ok;
}

int dxf_cpy_appid_drwg(dxf_drawing *source, dxf_drawing *dest){
	/* copy appid betweew drawings, only in use */
	int ok = 0, i, idx;
	dxf_node *current, *prev, *obj = NULL, *list[2], *appid, *appid_obj, *appid_name;
	
	list[0] = NULL; list[1] = NULL;
	if (source){
		list[0] = dest->ents;
		list[1] = dest->blks;
	}
	else return 0;
	
	for (i = 0; i< 2; i++){ /* look in BLOCKS and ENTITIES sections */
		obj = list[i];
		current = obj;
		while (current){ /* sweep elements in section */
			
			prev = current;
			if (current->type == DXF_ENT){
				appid_name = dxf_find_attr2(current, 1001); /* get element's appid */
				if (appid_name){
					appid_obj = dxf_find_obj_descr2(source->t_appid, "APPID",
            (char*)strpool_cstr2( &value_pool, appid_name->value.str));
					/* verify if APPID not exist */
					if (!dxf_find_obj_descr2(dest->t_appid, "APPID",
            (char*)strpool_cstr2( &value_pool, appid_name->value.str))){
						appid = dxf_ent_copy(appid_obj, dest->pool);					
						ok = ent_handle(dest, appid);
						if (ok) ok = dxf_obj_append(dest->t_appid, appid);
					}
				}
				/* search also in sub elements */
				if (current->obj.content){
					/* starts the content sweep */
					current = current->obj.content;
					continue;
				}
			}
			ok = 1;
			current = current->next; /* go to the next in the list*/

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
					if(prev == obj){
						current = NULL;
						break;
					}
					
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
