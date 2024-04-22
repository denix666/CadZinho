#include "util.h"

void str_upp(char *str) { /* upper case the string */
	while (*str= toupper(*str)) str++;
}

char * trimwhitespace(char *str){
	/* Note: This function returns a pointer to a substring of the original string.
	If the given string was allocated dynamically, the caller must not overwrite
	that pointer with the returned value, since the original pointer must be
	deallocated using the same allocator with which it was allocated.  The return
	value must NOT be deallocated using free() etc. 
	Author = Adam Rosenfield
	http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
	*/
	char *end;

	// Trim leading space
	while(isspace((unsigned char)*str)) str++;

	if(*str == 0)  // All spaces?
	return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end--;

	// Write new null terminator
	*(end+1) = 0;

	return str;
}

char *get_filename(char *path){
	/* Get the filename with extension from path string.
	This function does not modify the original string,
	but the return string must be used imediatly */
	static char buf[PATH_MAX_CHARS+1];
	char *ret = NULL;
	
	strncpy(buf, path, PATH_MAX_CHARS);
	ret = strrchr(buf, DIR_SEPARATOR);
	
	if (ret) ret++;
	else {
		//buf[0] = 0;
		ret = buf;
	}
	
	return ret;
}

char *get_dir(char *path){
	/* Get the directory from path string.
	This function does not modify the original string,
	but the return string must be used imediatly */
	static char buf[PATH_MAX_CHARS+1];
	char *ret = NULL;
	
	strncpy(buf, path, PATH_MAX_CHARS);
	ret = strrchr(buf, DIR_SEPARATOR);
	
	if (ret){
		ret++;
		*ret = 0; /* terminate string */
	}
	else {
		buf[0] = '.';
		buf[1] = DIR_SEPARATOR;
		buf[2] = 0;
	}
	
	return buf;
}

char *get_ext(char *path){
	/* Get the extension of file from path string.
	This function does not modify the original string,
	but the return string must be used imediatly.
	Return string is in lower case*/
	static char buf[PATH_MAX_CHARS+1];
	char *ret = NULL;
	int i;
	
	if(!path){
		buf[0] = 0;
		return buf;
	}
	
	strncpy(buf, path, PATH_MAX_CHARS);
	ret = strrchr(buf, '.');
	
	if (ret) {
		ret++;
		i = 0;
		while(ret[i]){
			ret[i] = tolower(ret[i]);
			i++;
		}
	}
	else {
		buf[0] = 0;
		ret = buf;
	}
	
	return ret;
}

void strip_ext(char *filename){
	/* Strip extension from filename string.
	Atention: This function modify the original string */
	char *ret = NULL;
	if (filename){
		ret = strrchr(filename, '.');
		if (ret){
			int pos = (int)(ret - filename);
			filename[pos] = 0;
		}
	}
}

char *escape_path(char *path){
	/* Escape windows paths with backslash
	This function does not modify the original string,
	but the return string must be used imediatly */
	static char buf[PATH_MAX_CHARS+1];
	char curr;
	int i, idx;
	
	idx = 0;
	for(i = 0; i < PATH_MAX_CHARS+1; i++){
		curr = path[i];
		
		if (curr == '\\'){
			buf[idx] = curr;
			idx++;
		}
		buf[idx] = curr;
		idx++;
		if (!curr) break; /* end string */
		if (idx >= PATH_MAX_CHARS - 1) break; /* buffer limit */
	}
	buf[idx] = 0; /* terminate buffer string */
	return buf;
}

int file_exists(char *fname){
	FILE *file;
	if (file = fopen(fname, "r")){
		fclose(file);
		return 1;
	}
	return 0;
}

int utf8_to_codepoint(char *utf8_s, int *uni_c){
	int ofs = 0;
	
	if (!utf8_s || !uni_c) return 0;
	
	char c = utf8_s[ofs];
	if (!c) return 0; /*string end*/
	*uni_c = 0;
	if ( (c & 0x80) == 0 ){
		*uni_c = c;
		ofs++;
	}
	else if ( (c & 0xE0) == 0xC0 ){
		if (!utf8_s[ofs+1]) return 0; /* error -> string end*/
		*uni_c = (utf8_s[ofs] & 0x1F) << 6;
		*uni_c |= (utf8_s[ofs+1] & 0x3F);
		ofs += 2;
	}
	else if ( (c & 0xF0) == 0xE0 ){
		if (!utf8_s[ofs+1] || !utf8_s[ofs+2]) return 0; /* error -> string end*/
		*uni_c = (utf8_s[ofs] & 0xF) << 12;
		*uni_c |= (utf8_s[ofs+1] & 0x3F) << 6;
		*uni_c |= (utf8_s[ofs+2] & 0x3F);
		ofs += 3;
	}
	else if ( (c & 0xF8) == 0xF0 ){
		if (!utf8_s[ofs+1] || !utf8_s[ofs+2] || !utf8_s[ofs+3]) return 0; /* error -> string end*/
		*uni_c = (utf8_s[ofs] & 0x7) << 18;
		*uni_c |= (utf8_s[ofs+1] & 0x3F) << 12;
		*uni_c |= (utf8_s[ofs+2] & 0x3F) << 6;
		*uni_c |= (utf8_s[ofs+3] & 0x3F);
		ofs += 4;
	}

	return ofs;

}

int codepoint_to_utf8(int uni_c, char utf8_s[5]){
	
	if (!utf8_s) return 0;
	
	int len = 0;
	utf8_s[4] = 0;
	if ( 0 <= uni_c && uni_c <= 0x7f ){
		utf8_s[0] = (char)uni_c;
		len++;
	}
	else if ( 0x80 <= uni_c && uni_c <= 0x7ff ){
		utf8_s[0] = ( 0xc0 | (uni_c >> 6) );
		utf8_s[1] = ( 0x80 | (uni_c & 0x3f) );
		len += 2;
	}
	else if ( 0x800 <= uni_c && uni_c <= 0xffff ){
		utf8_s[0] = ( 0xe0 | (uni_c >> 12) );
		utf8_s[1] = ( 0x80 | ((uni_c >> 6) & 0x3f) );
		utf8_s[2] = ( 0x80 | (uni_c & 0x3f) );
		len += 3;
	}
	else if ( 0x10000 <= uni_c && uni_c <= 0x1fffff ){
		utf8_s[0] = ( 0xf0 | (uni_c >> 18) );
		utf8_s[1] = ( 0x80 | ((uni_c >> 12) & 0x3f) );
		utf8_s[2] = ( 0x80 | ((uni_c >> 6) & 0x3f) );
		utf8_s[3] = ( 0x80 | (uni_c & 0x3f) );
		len += 4;
	}
	
	return len;
}

int str_utf2cp(char *str, int *cp, int max){
	int ofs = 0, str_start = 0, code_p, size = 0;
	
	while ((ofs = utf8_to_codepoint(str + str_start, &code_p)) && (size < max)){
		str_start += ofs;
		cp[size] = code_p;
		size++;
	}
	return size;
}

char *str_replace(char *orig, char *rep, char *with) {
	/*
	Author: jmucchiello
	http://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
	*/
	/* You must free the result if result is non-NULL. */
	char *result; /* the return string */
	char *ins;    /* the next insert point */
	char *tmp;    /* varies */
	int len_rep;  /* length of rep (the string to remove) */
	int len_with; /* length of with (the string to replace rep with) */
	int len_front; /* distance between rep and end of last rep */
	int count;    /* number of replacements */

	/* sanity checks and initialization */
	if (!orig || !rep)
		return NULL;
	len_rep = strlen(rep);
	if (len_rep == 0)
		return NULL; /* empty rep causes infinite loop during count */
	if (!with)
		with = "";
	len_with = strlen(with);

	/* count the number of replacements needed */
	ins = orig;
	for (count = 0; tmp = strstr(ins, rep); ++count) {
		ins = tmp + len_rep;
	}

	tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

	if (!result)
		return NULL;

	/*
	first time through the loop, all the variable are set correctly
	from here on, tmp points to the end of the result string
	ins points to the next occurrence of rep in orig
	orig points to the remainder of orig after "end of rep"
	*/
	while (count--) {
		ins = strstr(orig, rep);
		len_front = ins - orig;
		tmp = strncpy(tmp, orig, len_front) + len_front;
		tmp = strcpy(tmp, with) + len_with;
		orig += len_front + len_rep; /* move to next "end of rep" */
	}
	strcpy(tmp, orig);
	return result;
}

/*
struct Mem_buffer * manage_buffer (long size, enum buffer_action action){
	static struct Mem_buffer buf = {.buffer = NULL, .size = 0, .used = 0};
	
	if (action == BUF_GET){
		if (size <= 0) return NULL;
		if (buf.used != 0) return NULL;
		
		if (buf.buffer == NULL){
			if (buf.buffer = malloc (size)){
				buf.size = size;
			}
			else {
				buf.size = 0;
				buf.used = 0;
				return NULL;
			}
		}
		else if (size > buf.size){
			if (buf.buffer = realloc (buf.buffer, size)){
				buf.size = size;
			}
			else {
				buf.size = 0;
				buf.used = 0;
				return NULL;
			}
		}
		buf.used = 1;
	}
	else if (action == BUF_RELEASE){
		buf.used = 0;
	}
	else if (action == BUF_FREE){
		if (buf.buffer != NULL) free(buf.buffer);
		buf.buffer = NULL;
		buf.size = 0;
		buf.used = 0;
		return NULL;
	}
	
	
	return &buf;
}*/

struct Mem_buffer * manage_buffer (long size, enum buffer_action action, int idx){
	static struct Mem_buffer buf[4] = {
		{.buffer = NULL, .size = 0, .used = 0},
		{.buffer = NULL, .size = 0, .used = 0},
		{.buffer = NULL, .size = 0, .used = 0},
		{.buffer = NULL, .size = 0, .used = 0},
	};
	
	if (idx < 0 && idx > 3) return NULL;
	
	if (action == BUF_GET){
		if (size <= 0) return NULL;
		if (buf[idx].used != 0) return NULL;
		
		if (buf[idx].buffer == NULL){
			if (buf[idx].buffer = malloc (size)){
				buf[idx].size = size;
			}
			else {
				buf[idx].size = 0;
				buf[idx].used = 0;
				return NULL;
			}
		}
		else if (size > buf[idx].size){
			if (buf[idx].buffer = realloc (buf[idx].buffer, size)){
				buf[idx].size = size;
			}
			else {
				buf[idx].size = 0;
				buf[idx].used = 0;
				return NULL;
			}
		}
		buf[idx].used = 1;
	}
	else if (action == BUF_RELEASE){
		buf[idx].used = 0;
	}
	else if (action == BUF_FREE){
		if (buf[idx].buffer != NULL) free(buf[idx].buffer);
		buf[idx].buffer = NULL;
		buf[idx].size = 0;
		buf[idx].used = 0;
		return NULL;
	}
	
	
	return &buf[idx];
}

struct Mem_buffer *  load_file_reuse(char *path, long *fsize){
	FILE *file;
	
	*fsize = 0;
	file = fopen(path, "rb");
	if(file == NULL){
		return NULL;
	}
	
	fseek(file, 0, SEEK_END);
	*fsize = ftell(file); /* size of file*/
	fseek(file, 0, SEEK_SET);  //same as rewind(f);
	//printf("file size = %d\n", fsize);
	
	//char *buf = malloc(*fsize + 1);
	struct Mem_buffer *buf = manage_buffer(*fsize + 1, BUF_GET, 0);
	if (!buf || !buf->buffer){
		*fsize = 0;
		fclose(file);
		return NULL;
	}
	fread(buf->buffer, *fsize, 1, file);
	fclose(file);
	buf->buffer[*fsize] = 0;
	return buf;
}

char * load_file(char *path, long *fsize){
	FILE *file;
	
	*fsize = 0;
	file = fopen(path, "rb");
	if(file == NULL){
		return NULL;
	}
	
	fseek(file, 0, SEEK_END);
	*fsize = ftell(file); /* size of file*/
	fseek(file, 0, SEEK_SET);  //same as rewind(f);
	//printf("file size = %d\n", fsize);
	
	char *buf = malloc(*fsize + 1);
	if (!buf){
		*fsize = 0;
		fclose(file);
		return NULL;
	}
	fread(buf, *fsize, 1, file);
	fclose(file);
	buf[*fsize] = 0;
	return buf;
}

char * try_load_dflt(char *path, char *dflt){
	/* try to load a file to a buffer. If file fails, load a default string */
	long fsize;
	
	char *buf = load_file(path, &fsize); /* first, try file */
	
	if (buf == NULL && dflt != NULL) {
		/* if no file, then copy default string to buffer */
		fsize = strlen(dflt) + 1;
		buf = calloc(fsize, 1);
		if (buf) memcpy (buf, dflt, fsize);
	}
	
	return buf;
}

int miss_file (char *path, char *dflt){
	/* create a file to path, if file not exists. Use contents of default passsed string */
	
	/* verify if path is a valid string */
	if (!path) return 0;
	if (strlen(path) < 1) return 0;
	
	if (file_exists(path)) return 1; /* file exists, nothing to do. Return success */
	
	/* verify if default string is valid */
	if (!dflt) return 0;
	if (strlen(dflt) < 1) return 0;
	
	FILE *file = fopen(path, "w"); /* try to create file */
	if (file){
		/* then write contents of default string */
		fprintf(file, "%s", dflt);
		
		fclose(file);
		return 1; /* success */
	}
	
	return 0; /* fail */
}

/*-----------------------------------------*/

/*
Get to know your operating system

MIT licensed.
Copyright (c) Abraham Hernandez <abraham@abranhe.com>
modified by Zecruel - Cadzinho - https://stackoverflow.com/a/5920028/14506399
*/
const char * operating_system() {
	#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)|| defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
		/*define something for Windows (32-bit and 64-bit, this part is common)*/
		#ifdef _WIN64
			/* define something for Windows (64-bit only) */
			return "win64";
		#else
			/* define something for Windows (32-bit only) */
			return "win32";
		#endif
  #elif __EMSCRIPTEN__
    return "emscripten";
	#elif __APPLE__ || __MACH__
		#include <TargetConditionals.h>
		#if TARGET_IPHONE_SIMULATOR
			/* iOS Simulator */
		#elif TARGET_OS_IPHONE
			/* iOS device */
		#elif TARGET_OS_MAC
			/* Other kinds of Mac OS */
		#else
			/* Unknown Apple platform */
		#endif
		return "macOS";
	#elif __linux__
		return "linux";
	#elif __FreeBSD__
		return "freeBSD";
	#elif __unix || __unix__ /* all unices not caught above */
		return "unix";
	#elif defined(_POSIX_VERSION)
		return "unix";
	#else
		return "other";
	#endif
}

/*
Open URLS in C

MIT licensed.
Copyright (c) Abraham Hernandez <abraham@abranhe.com>
Modified by Zecruel - CadZinho
*/

int opener(const char *url) {
    
	const char *platform = operating_system();
	const char *cmd = NULL;

	/* Hanlde macOS */
	if (!strcmp(platform, "macOS")) {
		cmd = "open";
	}
	/* Handle Windows */
	else if (!strcmp(platform, "win32") || !strcmp(platform, "win64")) {
		cmd = "start";
	}
	/* Handle Linux, Unix, etc */
	else if (!strcmp(platform, "unix")
	|| !strcmp(platform, "linux") 
	|| !strcmp(platform, "freeBSD") 
	|| !strcmp(platform, "other")) {
		cmd = "xdg-open";
	}

	char script[PATH_MAX_CHARS] = "";
	strncat(script, cmd, PATH_MAX_CHARS - 1);
	strncat(script, " ", PATH_MAX_CHARS - 1);
	strncat(script, url, PATH_MAX_CHARS - 1 - strlen(script));
	
	system(script);
  //WinExec("your cmd command", SW_HIDE); instead of system("cmd command").
  //ShellExecute(0,"open", "notepad", "c:\\debug.txt", NULL, SW_SHOW);

	return 1;
}

/*------------------------------------------------------*/

const char * dflt_fonts_dir (){
	const char *platform = operating_system();

	/* Hanlde macOS */
	if (!strcmp(platform, "macOS")) {
		return "/Library/Fonts/:/System/Library/Fonts/";
	}
	/* Handle Windows */
	else if (!strcmp(platform, "win32") || !strcmp(platform, "win64")) {
		char *windir = getenv("WINDIR");
		if (windir){
			static char fntdir[PATH_MAX_CHARS+1] = "";
			strncat(fntdir, windir, PATH_MAX_CHARS);
			strncat(fntdir, "\\Fonts\\", PATH_MAX_CHARS - strlen(fntdir));
			return fntdir;
		}
		return "C:\\Windows\\Fonts\\";
	}
	/* Handle Linux, Unix, etc */
	else if (!strcmp(platform, "linux")){
		char *home = getenv("HOME");
		if (home){
			static char fntdir[PATH_MAX_CHARS+1] = "";
			strncat(fntdir, home, PATH_MAX_CHARS);
			int len = PATH_MAX_CHARS - strlen(fntdir);
			if (len > 0) strncat(fntdir, "/.local/share/fonts/:", len);
			len = PATH_MAX_CHARS - strlen(fntdir);
			if (len > 0) strncat(fntdir, home, len);
			len = PATH_MAX_CHARS - strlen(fntdir);
			if (len > 0) strncat(fntdir, "/.local/share/fonts/TTF/:", len);
			len = PATH_MAX_CHARS - strlen(fntdir);
			if (len > 0) strncat(fntdir, "/usr/share/fonts/:/usr/share/fonts/TTF/", len);
			
			return fntdir;
		}
		return "/usr/share/fonts/:/usr/share/fonts/TTF/";
	}
	/* Handle Unix and others */
	else if (!strcmp(platform, "unix")
	|| !strcmp(platform, "freeBSD") 
	|| !strcmp(platform, "other")) {
		return "/usr/share/fonts/";
	}
	return "/usr/share/fonts/";
}

const char * plat_dflt_font (){
	const char *platform = operating_system();

	/* Hanlde macOS */
	if (!strcmp(platform, "macOS")) {
		return "SF-Pro.ttf";
	}
	/* Handle Windows */
	else if (!strcmp(platform, "win32") || !strcmp(platform, "win64")) {
		return "arial.ttf";
	}
	/* Handle Linux, Unix, etc */
	else if (!strcmp(platform, "linux")){
		return "Vera.ttf";
	}
	/* Handle Unix and others */
	else if (!strcmp(platform, "unix")
	|| !strcmp(platform, "freeBSD") 
	|| !strcmp(platform, "other")) {
		return "Vera.ttf";
	}
	return "Vera.ttf";
}

int is_arabic(int cp){ /* verify if unicode code point is an arabic letter */
	return (cp > 1568 && cp < 1611);
}

static int arabic (int prev, int curr, int next){
	/* Return the corresponding glyph (cursive form) of a unicode arabic "character", 
	according its context (beginning, middle or ending in "word", or a isolated letter) */
	
	/* https://en.wikipedia.org/wiki/Arabic_script_in_Unicode */
	/* https://en.wikipedia.org/wiki/Arabic_Presentation_Forms-B */
	/* equivalents of code points from 1569 to 1610 */
	static int arabic_form_b[][4] = {/*0 = Isolated, 1 = End, 2 = Middle, 3 = Beginning  */
		{65152,65152,0,0}, /* Hamza  */
		{65153,65154,0,0}, /* Alef With Madda Above  */
		{65155,65156,0,0}, /* Alef With Hamza Above  */
		{65157,65158,0,0}, /* Waw With Hamza Above  */
		{65159,65160,0,0}, /* Alef With Hamza Below  */
		{65161,65162,65164,65163}, /* Yeh With Hamza Above  */
		{65165,65166,0,0}, /* Alef  */
		{65167,65168,65170,65169}, /* Beh  */
		{65171,65172,0,0}, /* Teh Marbuta  */
		{65173,65174,65176,65175}, /* Teh  */
		{65177,65178,65180,65179}, /* Theh  */
		{65181,65182,65184,65183}, /* Jeem  */
		{65185,65186,65188,65187}, /* Hah  */
		{65189,65190,65192,65191}, /* Khah  */
		{65193,65194,0,0}, /* Dal  */
		{65195,65196,0,0}, /* Thal  */
		{65197,65198,0,0}, /* Reh  */
		{65199,65200,0,0}, /* Zain  */
		{65201,65202,65204,65203}, /* Seen  */
		{65205,65206,65208,65207}, /* Sheen  */
		{65209,65210,65212,65211}, /* Sad  */
		{65213,65214,65216,65215}, /* Dad  */
		{65217,65218,65220,65219}, /* Tah  */
		{65221,65222,65224,65223}, /* Zah  */
		{65225,65226,65228,65227}, /* Ain  */
		{65229,65230,65232,65231}, /* Ghain  */
		{0,0,0,0}, /* Keheh With Two Dots Above  */
		{0,0,0,0}, /* Keheh With Three Dots Below  */
		{0,0,0,0}, /* Farsi Yeh With Inverted V  */
		{0,0,0,0}, /* Farsi Yeh With Two Dots Above  */
		{0,0,0,0}, /* Farsi Yeh With Three Dots Above  */
		{1600,1600,1600,1600}, /* Arabic Tatweel */
		{65233,65234,65236,65235}, /* Feh  */
		{65237,65238,65240,65239}, /* Qaf  */
		{65241,65242,65244,65243}, /* Kaf  */
		{65245,65246,65248,65247}, /* Lam  */
		{65249,65250,65252,65251}, /* Meem  */
		{65253,65254,65256,65255}, /* Noon  */
		{65257,65258,65260,65259}, /* Heh  */
		{65261,65262,0,0}, /* Waw  */
		{65263,65264,0,0}, /* Alef Maksura  */
		{65265,65266,65268,65267}, /* Yeh  */
	};
	static int ligature[][2]={
		{0, 0},
		{65269, 65270}, /*ligature Lam/Alef With Madda Above  */
		{65271, 65272}, /*ligature Lam/Alef With Hamza Above  */
		{0, 0},
		{65273, 65274}, /*ligature Lam/Alef With Hamza Below  */
		{0, 0},
		{65275, 65276} /*ligature Lam/Alef  */
	};
	
	/* Alef indexes = 1,2,4,6
	Lam index = 35 */
	
	/* get indexes of letters, to lookup table */
	if (!is_arabic(curr)) return curr;
	curr -= 1569;
	
	if (is_arabic(prev)) prev -= 1569;
	else prev = -1;
	
	if (is_arabic(next)) next -= 1569;
	else next = -1;
	
	if (prev >= 0){ /* verify if letter is in middle or end */
		/* Lam/Alef ligature */
		if (prev == 35 && (curr == 1 || curr == 2 || curr == 4 || curr == 6)) 
			return 0; /* Lam/Alef previous procceced */
		if (arabic_form_b[prev][2]){
			if (next >= 0 && arabic_form_b[curr][2]){
				return arabic_form_b[curr][2]; /* middle letter */
			}
			return arabic_form_b[curr][1]; /* ending letter */
		}			
	}
	if (next >= 0){ /* verify if letter is in beginning */
		/* special case - Lam/Alef ligature */
		if (curr == 35 && (next == 1 || next == 2 || next == 4 || next == 6)) {
			if (prev >= 0 && arabic_form_b[prev][2]){
				return ligature[next][1]; /* ligature at end */
			}
			return ligature[next][0]; /* isolated ligature*/
		} /* **** */
		if(arabic_form_b[curr][3]) return arabic_form_b[curr][3]; /* beginning letter */
	}
	return arabic_form_b[curr][0]; /* isolated letter */
}

int is_devanagari(int cp){
	return (cp > 2303 && cp < 2432);
}

static int devanagari (int prev, int curr, int next){
	if (next == 2367 || next == 2382){
		return next;
	}
	if (curr == 2367 || curr == 2382){
		return prev;
	}
	return curr;
}

int is_bengali(int cp){
	return (cp > 2431 && cp < 2559);
}

static int bengali (int prev, int curr, int next){
	if (next == 2495 || next == 2503 || next == 2504){
		return next;
	}
	if (curr == 2495 || curr == 2503 || curr == 2504){
		return prev;
	}
	/* ????? - 3 code points?
	prev + 0x9cb = 0x9c7 + prev + 0x9be
	prev + 0x9cc = 0x9c7 + prev + 0x9d7
	???? */
	if (next == 2507 || next == 2508){
		return next;
	}
	if (curr == 2507 || curr == 2508){
		return prev;
	}
	/* ?????? */
	
	return curr;
}

int contextual_codepoint (int prev, int curr, int next){
	if (is_arabic(curr)) {
		return arabic (prev, curr, next);
	}
	if (is_devanagari(curr)) {
		return devanagari (prev, curr, next);
	}
	if (is_bengali(curr)) {
		return bengali (prev, curr, next);
	}
	return curr;
}

/* ------------------------------------------- */

int dir_check(char *path) { /* verify if directory exists */
	/* check if path is a valid string */
	if (!path) return 0;
	if (strlen(path) < 1) return 0;
	
	DIR* dir = opendir(path);
	if (dir) {
		closedir(dir);
		return 1; /* success */
	}
	return 0; /* fail */
}

char * dir_full(char *path) { /* get full path of a folder */
	static char full_path[PATH_MAX_CHARS+1];
	full_path[0] = 0; /*init */
	
	/* check if path is a valid folder */
	if (!dir_check(path)) return full_path;
	
	/* Directory exists. */
	
	/* get curent directory */
	char curr_path[PATH_MAX_CHARS+1];
	getcwd(curr_path, PATH_MAX_CHARS);
	
	chdir(path); /* change working dir to path*/
	
	/* put dir separator at end of returnned string */
	getcwd(full_path, PATH_MAX_CHARS);
	int len = strlen (full_path) ;
	if (len < PATH_MAX_CHARS - 1){
		if (full_path[len - 1] != DIR_SEPARATOR){
			full_path[len] = DIR_SEPARATOR;
			full_path[len + 1]  = 0;
		}
	}
	else {
		full_path[0] = 0; /* fail - full_path is truncated */
	}
	
	chdir(curr_path); /* change working back */
	
	return full_path;
}

int dir_change( char *path) { /* change current directory */
	/* check if path is a valid string */
	if (!path) return 0;
	if (strlen(path) < 1) return 0;
	
	int ret = chdir(path); 
	if (!ret) return 1; /* success */
	return 0; /* fail */
}

int dir_make (char *path) {
	/* check if path is a valid string */
	if (!path) return 0;
	if (strlen(path) < 1) return 0;
	
	#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)|| defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
		int ret = _mkdir(path);
	#else
		int ret = mkdir(path, 0777);
	#endif
	if (!ret) return 1; /* success */
	return 0; /* fail */
}

int dir_miss (char* path){ /* try to create a folder, if not exists */
	/* check if path is a valid string */
	if (!path) return 0;
	if (strlen(path) < 1) return 0;
	
	if (dir_check(path)) return 1; /* folder already exists */
	return dir_make (path); /* try to create folder */
}


void matrix4_mul(float *mat_a, float *mat_b, float *mat_r) {
    for (unsigned int i = 0; i < 16; i += 4)
        for (unsigned int j = 0; j < 4; ++j)
            mat_r[i + j] = (mat_b[i + 0] * mat_a[j +  0])
                            + (mat_b[i + 1] * mat_a[j +  4])
                            + (mat_b[i + 2] * mat_a[j +  8])
                            + (mat_b[i + 3] * mat_a[j + 12]);
}

int invert_4matrix(float *m, float *m_inv){
    double inv[16], det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return 0;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        m_inv[i] = inv[i] * det;

    return 1;
}

/* calcule intersect point between ray and plane */
/* return 1 success, or 0 if not intersect (ray paralel to plane) */
int ray_plane(double ray_o[3], /*ray orign point*/
	double ray_dir[3], /* ray direction vector */
	double plane[4], /* plane, definied by plane[0]*x + plane[1]*y + plane[2]*z + plane[3] = 0 */
	double point[3] /* result point (returned value) */
){
	double t, den = 0;
	
	den = ray_dir[0] * plane[0] + ray_dir[1] * plane[1] + ray_dir[2] * plane[2];
	
	if (fabs(den) < 1e-9) return 0; /* no intersection */
	
	t = -(ray_o[0] * plane[0] + ray_o[1] * plane[1] + ray_o[2] * plane[2] 
		+ plane[3]) / den;
	
	point[0] = ray_dir[0] * t + ray_o[0];
	point[1] = ray_dir[1] * t + ray_o[1];
	point[2] = ray_dir[2] * t + ray_o[2];
	
	return 1;
}

double time_to_julian(time_t t){
  return 2440587.5 + (double) t / 86400.0;
}

time_t julian_to_time(double jd){
  return (time_t) (jd - 2440587.5) * 86400;
}

unsigned int hash (unsigned int value){
	
	static unsigned int multiplier = 0x43b0d7e5u;

  value ^= multiplier;
  multiplier *= 0x931e8875u;
  value *= multiplier;
  value ^= value >> 16;
  return value;
}

void uuid_generate(unsigned int number[4]) {
  static float count = 0; /* internal counter */
	time_t t;
	time(&t); /* get current time */
	int nr = rand(); /* random number */
  
  number[0] = hash (t);
  number[1] = hash ((unsigned int) count); 
  number[2] = hash (nr);
  number[3] = hash (nr + (unsigned int) count);
  count += 1.2345;
}

void uuid4_str(char *dst, unsigned int *num, const char *templ) {
  if (!templ) templ = "{xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx}";
  static const char *chars = "0123456789ABCDEF";
  unsigned char *b = (unsigned char *) num;
  const char *p;
  int i, n;
  
  /* build string */
  p = templ;
  i = 0;
  while (*p) {
    if (i < 32){
      n = b[i >> 1];
      n = (i & 1) ? (n >> 4) : (n & 0xf);
    }
    switch (*p) {
      case 'x'  : *dst = chars[n];              i++;  break;
      case 'y'  : *dst = chars[(n & 0x3) + 8];  i++;  break;
      default   : *dst = *p;
    }
    dst++, p++;
  }
  *dst = '\0';
}