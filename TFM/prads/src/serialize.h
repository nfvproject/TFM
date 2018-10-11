/*
  serialize.h
  C serialization library
  version 0.4
  ulf.astrom@gmail.com / happyponyland.net

  This is a library for serializing structures in C.  Please see
  readme.html for more information and functions.html for a function
  reference.

  Copyright (c) 2012, 2013 Ulf Åström

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any
  damages arising from the use of this software.

  Permission is granted to anyone to use this software for any
  purpose, including commercial applications, and to alter it and
  redistribute it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must
     not claim that you wrote the original software. If you use this
     software in a product, an acknowledgment in the product
     documentation would be appreciated but is not required.

  2. Altered source versions must be plainly marked as such, and must
     not be misrepresented as being the original software.

  3. This notice may not be removed or altered from any source
     distribution.
*/

#ifndef SERIALIZE_H
#define SERIALIZE_H

/* Uncomment this to get verbose debugging information */
//#define SER_DEBUG

#ifdef SER_DEBUG
#define SER_DPRINT(...) fprintf(stdout, __VA_ARGS__);
#else
#define SER_DPRINT(...)
#endif

#define SER_MSGLEN 250

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SER_IF_SIZEOF(n)   if (strcmp(#n, type) == 0)  return sizeof(n);

#define SER_OPT_DEFAULT 0
#define SER_OPT_COMPACT 1

typedef struct ser_job_t ser_job_t;
typedef struct ser_tra_t ser_tra_t;
typedef struct ser_field_t ser_field_t;
typedef struct ser_holder_t ser_holder_t;
typedef struct ser_subst_ptr_t ser_subst_ptr_t;

enum ser_tok_t
{
  ser_tok_err = 0,
  ser_tok_end = 1,
  ser_tok_id = 2,
  ser_tok_open = 3,
  ser_tok_close = 4,
  ser_tok_term = 5,
  ser_tok_string = 6,
  ser_tok_any = 7,
  ser_tok_int = 8,
  ser_tok_char = 9,
  ser_tok_sep = 10
};

enum ser_atype_t
{
  ser_flat = 0,
  ser_array_dynamic = 1,
  ser_array_nullterm = 2
};

typedef enum ser_tok_t ser_tok_t;
typedef enum ser_atype_t ser_atype_t;

struct ser_job_t
{
  ser_holder_t * holder;
  size_t things;
  size_t allocated;

  uint16_t options;

  char * result;
  int result_len;

  ser_tra_t * first_tra;

  char * p_str;
  char * p_pos;
  char * p_end;
  size_t p_byte;

  ser_subst_ptr_t * first_sptr;

  void (*log_func)(char *);
};

struct ser_tra_t
{
  char * id;
  ser_atype_t atype;
  size_t size;
  ser_tra_t * next;
  ser_field_t * first_field;
  int (*custom)(ser_job_t *, ser_tra_t *, void *);
};

struct ser_field_t
{
  int ref;
  char * tag;
  char * type;
  ser_field_t * next;
  ptrdiff_t offset;
  size_t repeat;
};

struct ser_holder_t
{
  char * type;
  size_t elements;
  void * start;
  size_t size;
  size_t replace_id;
  char * replace_field;
};

struct ser_subst_ptr_t
{
  size_t d_hi;
  char * d_tag;
  size_t d_ai;
  ser_subst_ptr_t * next;
};


extern char * ser_token_codes[];
char * ser_token_code(const ser_tok_t token);


/* Translators */
ser_tra_t * ser_new_tra(const char * id, const size_t size, ser_tra_t * att);
void ser_del_tra(ser_tra_t * tra);
ser_tra_t * ser_find_tra(ser_tra_t * tra, const char * id);
void ser_tra_list(ser_tra_t * tra);
ser_tra_t * ser_new_dyn_array(const char * id, const char * type,
			      const int ref, const size_t size, ser_tra_t * att);
ser_tra_t * ser_new_nullterm_array(const char * id, const char * type,
				     const int ref, ser_tra_t * att);
void ser_set_custom_handler(ser_tra_t * tra,
			    int (*func_ptr)(ser_job_t *, ser_tra_t *, void *));
char * ser_meta_tra(ser_tra_t * tra);


/* Fields */
ser_field_t * ser_new_field(ser_tra_t * tra, const char * type, const int ref,
			      const char * tag, const size_t offset);
ser_field_t * ser_find_field(ser_tra_t * tra, const char * id, const char * tag);
ser_field_t * ser_field_by_offset(ser_tra_t * tra,
				  const size_t offset);
void ser_del_field(ser_tra_t * tra, ser_field_t * field);
ser_field_t * ser_find_array_count(ser_tra_t * tra, char * array_tag);
long ser_numeric_cast(void * ptr, char * type);


/* Jobs */
void ser_del_subst_ptrs(ser_job_t * job);
bool ser_check_subst_ptrs(ser_job_t * job);
ser_subst_ptr_t * ser_add_subst_ptr(ser_job_t * job, const size_t d_hi,
				    const char * d_tag, const size_t d_ai);
void * ser_resolve_subst_ptr(ser_job_t * job, const size_t hi,
			     const char * tag, const size_t ai,
			     char ** type, bool * ref);


/* Deflating */
char * ser_ialize(ser_tra_t * tra, char * type, void * first,
		  void (*log_func)(char *), uint16_t options);
bool ser_dump(ser_job_t * job, char * type, void * thing);
bool ser_write_struct(ser_job_t * job, ser_tra_t * tra,
		      size_t ptr_index, void * thing,
		      const int anon);
bool ser_write_array(ser_job_t * job, ser_tra_t * tra, ser_field_t * field,
		     void * array_start, size_t elements);
bool ser_write_value(ser_job_t * job, const char * type,
		     const int ref, void * field_ptr);
bool ser_write_primitive(ser_job_t * job, const size_t holder,
			const char * type, void * field_ptr);
bool ser_follow_ptrs(ser_job_t * job, size_t holder_index,
		     ser_tra_t * thing_tra, void * thing);
void ser_follow_references(ser_job_t * job, ser_tra_t * tra,
			     size_t ptr_index, void * thing);
bool ser_job_realloc_holders(ser_job_t * job, const int n);
bool ser_job_realloc_result(ser_job_t * job, const int len);
size_t ser_assign_holder(ser_job_t * job, void * thing, size_t size);
size_t ser_find_holder(ser_job_t * job, void * thing, size_t size);
bool ser_make_reference(ser_job_t * job, const void * thing,
			const size_t size, const size_t skip, char ** dest);
size_t ser_nullterm_len(void * thing, char * type);
bool ser_job_cat(ser_job_t * job, char * s);
bool ser_element_count(ser_job_t * job, ser_tra_t * tra,
		      ser_field_t * field, void * thing, void * start);
bool ser_set_holder_elements(ser_job_t * job, size_t holder_index, size_t elements);
void ser_ptr_overlap(ser_job_t * job);
void ser_list_holders(ser_job_t * job);
void ser_clear_holder(ser_holder_t * holder);


/* Inflating */
void * ser_parse(ser_tra_t * first_tra, const char * expected_type,
		 char * s, void (*log_func)(char*));
ser_tok_t ser_ntok(ser_job_t * job, char ** dest, long int * value);
void * ser_blank_struct(ser_tra_t * tra);

bool ser_expect(ser_job_t * job, ser_tok_t expected);
bool ser_read_primitive(ser_job_t * job, const long id, const char * type);
bool ser_read_struct(ser_job_t * job, const long id, ser_tra_t * thing_tra);
bool ser_read_struct_internal(ser_job_t * job, ser_holder_t * holder,
			     ser_tra_t * thing_tra, void * struct_ptr);
bool ser_read_value(ser_job_t * job, const char * type, ser_tok_t r,
		   void * where, char * token, long int value);

void ser_write_unsigned(void * ptr, const char * type, const unsigned long uvalue);
void ser_write_signed(void * ptr, const char * type, const signed long svalue);
bool ser_restore_ptr(ser_job_t * job, ser_holder_t * holder,
		     ser_tra_t * tra, ser_field_t * field);
bool ser_restore_array_counts(ser_job_t * job, ser_holder_t * holder,
			      ser_tra_t * tra);
bool ser_restore_pointers(ser_job_t * job);

void * ser_replace_ptr(ser_job_t * job,
		       const size_t target_id,
		       const char * target_field);
void * ser_subst_array_start(ser_job_t * job, size_t id);
bool ser_dissect_reference(char * token, size_t * holder_dest,
			   char ** field_dest, size_t * index_dest);


/* Logging */
void ser_def_log(char * msg);
void ser_null_log(char * msg);


/* Types */
size_t ser_field_size(ser_job_t * job, const char * type, const int ref);
bool ser_is_primitive(const char * s);
bool ser_valid_type(ser_tra_t * tra, char * s);
bool ser_is_signed(const char * type);


/* Strings */
char * ser_escape_str(char * s);
char * ser_unescape_str(char * s);
char * ser_preformat(char * s);
char ser_hex2char(char * s);
  
#endif
