/*
  serialize.c
  C serialization library
  version 0.4
  ulf.astrom@gmail.com / happyponyland.net

  This is a library for serializing structures in C.
  Please see readme.html for more information.

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
/*
  ##doc-meta
  #title C serialization library: Function reference
  #catorder Translators
  #catorder Fields
  #catorder Serialization
  #catorder Logging
  #catorder Strings
  #catorder Types
  #catorder Internal
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "serialize.h"



/**
   Makes a new translator called @id for a structure @size bytes
   large. If @att is non-NULL the new translator will be attached to
   the end of that list of translators.
   
   Returns a pointer to the new translator or NULL on error.

   #category Translators
*/
ser_tra_t * ser_new_tra(const char * id, const size_t size, ser_tra_t * att)
{
  ser_tra_t * tra;
  char * dup_id;

  if (!id || size < 0)
    return NULL;

  if (att && ser_find_tra(att, id))
  {
    SER_DPRINT("translator %s already exists\n", id);
    return NULL;
  }
  
  SER_DPRINT("creating translator %s (size %ld)\n", id, size);

  tra = malloc(sizeof(ser_tra_t));
  dup_id = strdup(id);

  if (!tra || !dup_id)
  {
    free(tra);
    free(dup_id);
    return NULL;
  }

  tra->next = NULL;
  tra->id = dup_id;
  tra->size = size;
  tra->atype = ser_flat;
  tra->first_field = NULL;
  tra->custom = NULL;
  
  /* Attach it at the end. */
  if (att)
  {
    while (att->next != NULL)
      att = att->next;

    att->next = tra;
  }

  return tra;
}



/**
   Deletes the translator @tra and all translators linked after it.

   #category Translators
*/
void ser_del_tra(ser_tra_t * tra)
{
  ser_tra_t * tmp;

  if (!tra)
    return;

  do
  {
    tmp = tra->next;

    SER_DPRINT("deleting translator %s\n", tra->id);

    ser_del_field(tra, tra->first_field);
    
    free(tra->id);
    free(tra);

    tra = tmp;
  } while (tra);

  return;
}




/**
   Makes a new translator called @id that will be a dynamic
   (variable-size, i.e. a pointer) array of @type@. If @att is
   non-NULL the new translator will be attached to the end of that
   list of translators.

   Note: other translators referencing @id needs an array count field
   to function properly.

   Returns a pointer to the new translator or NULL on error.

   #category Translators
*/
ser_tra_t * ser_new_dyn_array(const char * id, const char * type,
			      const int ref, const size_t size, ser_tra_t * att)
{
  ser_tra_t * tra;
  ser_field_t * field;

  tra = ser_new_tra(id, size, att);

  if (tra)
  {
    field = ser_new_field(tra, type, ref, "__internal__", 0);

    if (!field)
    {
      ser_del_tra(tra);
      return NULL;
    }

    field->repeat = 0;

    tra->atype = ser_array_dynamic;

    SER_DPRINT("new dynamic array %s, size %ld\n", tra->id, tra->size);
  }

  return tra;
}



/**
   Makes a new translator called @id that will be a null-terminated
   array of @type@. If @att is non-NULL the new translator will be
   attached to the end of that list of translators.

   Returns a pointer to the new translator or NULL on error.

   #category Translators
*/
ser_tra_t * ser_new_nullterm_array(const char * id, const char * type,
				   const int ref, ser_tra_t * att)
{
  ser_tra_t * tra;
  ser_field_t * field;

  tra = ser_new_tra(id, ser_field_size(NULL, type, ref), att);

  if (tra)
  {
    field = ser_new_field(tra, type, ref, "__internal__", 0);

    if (!field)
    {
      ser_del_tra(tra);
      return NULL;
    }

    field->repeat = 0;

    tra->atype = ser_array_nullterm;

    SER_DPRINT("new null-terminated array %s\n", tra->id);
  }

  return tra;
}



/**
   Returns the length of a null-terminated array of @type starting at
   @thing@. Elements will be counted up to (and including) the first
   that is zero.

   #category Internal
*/
size_t ser_nullterm_len(void * thing, char * type)
{
  size_t el_w; /* element width */
  size_t value;
  size_t count;
  void * ptr;

  if (!thing || !type)
    return 0;

  count = 0;
  el_w = ser_field_size(NULL, type, 0);

  while (1)
  {
    ptr = thing + el_w * count;
    value = ser_numeric_cast(ptr, type);

    count++;

    if (value == 0)
      break;
  }

  return count;
}



/**
   Adds field @tag to the translator @tra@. The field will be
   serialized as @type@. If @ref is non-zero it will be made a
   reference. @offset is the number of bytes between the start of the
   structure and the start of the member field; it is recommended to
   find this using offsetof().
   
   The field will not be created if a field with the same tag exists
   in the translator or if the translator is defined as an array,
   i.e. it was created with ser_new_dyn_array() or
   ser_new_null_array() (see ser_tra_t.atype).
   
   Returns a pointer to the new field, or NULL on error.

   #category Fields
*/
ser_field_t * ser_new_field(ser_tra_t * tra, const char * type, const int ref,
			    const char * tag, const size_t offset)
{
  ser_field_t * field;
  char * dup_type;
  char * dup_tag;

  if (!tra || !tag || !type)
    return NULL;
  
  if (tra->atype != ser_flat)
    return NULL;

  if (ser_find_field(tra, tra->id, tag))
    return NULL;

  /* Set up the field structure */
  field = malloc(sizeof(ser_field_t));
  dup_type = strdup(type);
  dup_tag = strdup(tag);

  if (!field || !dup_type || !dup_tag)
  {
    free(dup_tag);
    free(dup_type);
    free(field);
    return NULL;
  }

  field->tag = dup_tag;
  field->type = dup_type;
  field->offset = offset;
  field->next = NULL;
  field->repeat = 1;
  field->ref = ref;

  /*
    Attach the field to the translator. If there are no fields in the
    translators, put it as first field. If there are fields already,
    put it at the end of the field list.
  */
  if (!tra->first_field)
  {
    tra->first_field = field;
  } 
  else
  {
    ser_field_t * temp;

    temp = tra->first_field;
    
    while (temp->next != NULL)
      temp = temp->next;

    temp->next = field;
  }

  SER_DPRINT("added field %s %s->%s (offset %ld)\n",
	     type, tra->id, tag, field->offset);

  return field;
}



/**
   Deletes @field from @tra and all fields linked after it.

   #category Fields
*/
void ser_del_field(ser_tra_t * tra, ser_field_t * field)
{
  ser_field_t * tmp;
  
  if (!tra || !field)
    return;

  SER_DPRINT("deleting field %s->%s\n", tra->id, field->tag);

  do
  {
    tmp = field->next;

    free(field->tag);
    free(field->type);
    free(field);

    field = tmp;
  } while (field);

  if (tra->first_field == field)
    tra->first_field = NULL;
  
  return;
}



/**
   Serializes translators. This function sets up translators for
   ser_tra_t and ser_field_t; i.e. it describes the structure of the
   translators themselves. If @tra is provided the translators in this
   list will be serialized, otherwise the meta translators will be
   serialized.

   Returns the serialized translator(s).

   #category Translators
*/
char * ser_meta_tra(ser_tra_t * tra)
{
  char * ret;
  ser_tra_t * fmtra; /* first "meta" tra */
  ser_tra_t * mtra; /* "meta" tra */

  fmtra = mtra = ser_new_tra("translator", sizeof(ser_tra_t), NULL);
  ser_new_field(mtra, "string",     0, "id",          offsetof(ser_tra_t, id));
  ser_new_field(mtra, "int",        0, "atype",       offsetof(ser_tra_t, atype));
  ser_new_field(mtra, "size_t",     0, "size",        offsetof(ser_tra_t, size));
  ser_new_field(mtra, "translator", 1, "next",        offsetof(ser_tra_t, next));
  ser_new_field(mtra, "field",      1, "first_field", offsetof(ser_tra_t, first_field));

  mtra = ser_new_tra("field", sizeof(ser_field_t), fmtra);
  ser_new_field(mtra, "string", 0, "tag",    offsetof(ser_field_t, tag));
  ser_new_field(mtra, "string", 0, "type",   offsetof(ser_field_t, type));
  ser_new_field(mtra, "int",    0, "ref",    offsetof(ser_field_t, ref));
  ser_new_field(mtra, "field",  1, "next",   offsetof(ser_field_t, next));
  ser_new_field(mtra, "size_t", 0, "offset", offsetof(ser_field_t, offset));
  ser_new_field(mtra, "size_t", 0, "repeat", offsetof(ser_field_t, repeat));
  
  if (tra)
    ret = ser_ialize(fmtra, "translator", tra, &ser_def_log, 0);
  else
    ret = ser_ialize(fmtra, "translator", fmtra, &ser_def_log, 0);

  ser_del_tra(fmtra);

  return ret;
}



/**
   Prints (to stderr) the fields in @tra and all linked translators.

   #category Translators
*/
void ser_tra_list(ser_tra_t * tra)
{
  ser_field_t * field;

  while (tra)
  {
    fprintf(stderr, "translator %s", tra->id);
    fprintf(stderr, "  /* size %ld */",  tra->size);
    fprintf(stderr, "\n{\n");

    for (field = tra->first_field; field; field = field->next)
    {
      fprintf(stderr, "  %s %s  ",
	      field->type, field->tag);
      
      if (field->repeat)
      {
	fprintf(stderr, "  /* r=%ld %ld-%ld */",
		field->repeat,
		field->offset,
		field->offset +
		ser_field_size(NULL, field->type, field->ref) * field->repeat - 1);
      }
	      
      fprintf(stderr, "\n");
    }

    fprintf(stderr, "}\n");

    tra = tra->next;
  }

  return;
}





/**
   Serializes @first as @type, using @tra as translator list (a
   translator for @type must exist in @tra). Extra options can be
   given as a bitmask in @options@; see SER_OPT_* for details.
   
   If @log_func is provided it will be called with error messages if
   serialization fails. If it is NULL output will be muted. See
   ser_def_log(). Implement your own to redirect output to a custom
   logging console, etc.
   
   Returns a string (which must be freed after use) describing the
   structure in human-readable format. Returns NULL on error.

   #category Serialization
*/
char * ser_ialize(ser_tra_t * tra, char * type, void * first, 
		  void (*log_func)(char *), uint16_t options)
{
  ser_tra_t * start_tra;
  ser_job_t job;
  char tmp[SER_MSGLEN];
  size_t i;
  bool failed = false;

  if (/*!tra ||*/ !type)
    return NULL;

  /*
    Instead of checking for the presence of a log function every time
    we wish to print we will just redirect all output to the "null"
    logger. Under normal operation this should not be called, so
    performance isn't an issue.
  */
  if (log_func)
    job.log_func = log_func;
  else
    job.log_func = &ser_null_log;

  job.first_tra = tra;

  job.options = options;
  job.holder = NULL;
  job.things = 2;
  job.allocated = 0;

  /* Output buffer */
  job.result = NULL;
  ser_job_realloc_result(&job, 0);
  job.result[0] = '\0';

  ser_job_realloc_holders(&job, 10);
  ser_clear_holder(&job.holder[1]);

  if (ser_is_primitive(type))
  {
    if (!ser_write_primitive(&job, 1, type, first))
      goto error_cleanup;

    goto finish;
  }
  else
  {
    start_tra = ser_find_tra(job.first_tra, type);
    
    if (start_tra == NULL)
    {
      snprintf(tmp, SER_MSGLEN, "warning: no translator for type %s", type);
      job.log_func(tmp);
      goto error_cleanup;
    }
  }
  
  job.holder[1].start = first;
  job.holder[1].type = strdup(type);
  job.holder[1].size = start_tra->size;

  /*
    Descend into the first structure and assign holders for all its
    pointers. Each will be added to the same list so the loop won't
    finish until all have been covered.
  */
  for (i = 1; i < job.things; i++)
  {
    SER_DPRINT("holder[%ld] (%ld)\n", i, job.things);

    if (ser_follow_ptrs(&job, i, NULL, NULL) == 0)
    {
      SER_DPRINT("follow_ptrs failed!\n");
      break;
    }
  }
  
  /* Check for pointers that overlap, redirect these if possible. */
  ser_ptr_overlap(&job);

  #ifdef SER_DEBUG
  ser_list_holders(&job);
  #endif

  /* Keep writing structures until we've covered them all. */
  SER_DPRINT("using %ld holders\n", job.things);

  for (i = 1; i < job.things; i++)
  {
    size_t replace;

    replace = job.holder[i].replace_id;

    if (replace)
    {
/*      char tmp[SER_MSGLEN];
      snprintf(tmp, SER_MSGLEN, "%s #%ld -> #%ld.%s;%s",
	       job.holder[i].type, i,
	       replace,
               job.holder[i].replace_field,
	       (job.options & SER_COMPACT ? "" : "\n"));
	       ser_job_cat(&job, tmp);*/
      continue;
    }

    if (ser_is_primitive(job.holder[i].type))
    {
      if (ser_write_primitive(&job, i, job.holder[i].type, job.holder[i].start) == 0)
	break;
    }
    else if (ser_dump(&job, job.holder[i].type, job.holder[i].start) == 0)
      break;
  }

finish:
  
  for (i = 1; i < job.things; i++)
  {
    free(job.holder[i].replace_field);
  }

  free(job.holder);

  if (failed)
  {
    free(job.result);
    return NULL;
  }

  return job.result;

error_cleanup:
  failed = true;
  goto finish;
}



/**
   Lists the holders in @job@; what they point to, size, type, element
   count, redirections, etc.

   #category Internal
 */
void ser_list_holders(ser_job_t * job)
{
  size_t i;

  if (!job)
    return;
  
  for (i = 1; i < job->things; i++)
  {
    SER_DPRINT("holder[%ld]:\n", i);
    SER_DPRINT("->start == %p\n", job->holder[i].start);
    SER_DPRINT("->size == %ld\n", job->holder[i].size);
    SER_DPRINT("->elements == %ld\n", job->holder[i].elements);

    if (job->holder[i].type)
    {
      SER_DPRINT("->type == %s\n", job->holder[i].type);
    }
    else
    {
      SER_DPRINT("->tra == NULL\n");
    }

    SER_DPRINT("->replace_id == %ld\n", job->holder[i].replace_id);

    if (job->holder[i].replace_field)
      SER_DPRINT("->replace_field == %s\n", job->holder[i].replace_field);
  }

  return;
}



/**
   Scans @job for holders whose pointers overlap. If a holder is found
   whose destination would fit within a another structure, it will be
   marked as a redirected structure. This information will be used by
   ser_ialize().

   Returns non-zero on success, zero on failure.
   
   #category Internal
*/
void ser_ptr_overlap(ser_job_t * job)
{
  size_t things;
  size_t i;

  if (!job)
    return;

  SER_DPRINT("scanning for pointer overlap\n");

  things = job->things;

  for (i = 1; i < things; i++)
  {
    if (ser_make_reference(job, job->holder[i].start, job->holder[i].size, i, NULL))
      job->holder[i].replace_id = 1;

    continue;
  }

  return;
}



/**
   Reallocates @job@s output buffer to @len characters. One extra byte
   will be reserved for the terminator; this does not need to be
   included in @len@.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_job_realloc_result(ser_job_t * job, const int len)
{
  char * new_result;

  if (!job)
    return false;

  new_result = realloc(job->result, len + 1);

  if (!new_result)
  {
    printf("\n\nshit\n\n");
    exit(1);
    return false;
  }

  job->result = new_result;
  job->result_len = len;

  return true;
}


/**
   Concatenates @s to @job@s result buffer. Note that the buffer might be
   moved and old pointers to it invalidated.
   
   Returns true on success, false on failure (bad pointers, memory
   could not be allocated, etc).

   #category Internal
*/
bool ser_job_cat(ser_job_t * job, char * s)
{
  int cat_len;
  int old_len;

  if (!job || !s)
    return false;

  old_len = job->result_len;
  cat_len = strlen(s);

  if (ser_job_realloc_result(job, old_len + cat_len) == 0)
    return false;

  strncpy(job->result + old_len, s, cat_len);

  job->result[job->result_len] = '\0';
  
  return true;
}


/**
   Finds the index of @thing in @job@s list of pointer holders, or adds it
   if it isn't found. If the list is too small to hold the addition it
   will be resized aggressively.
   
   Returns the index assigned or 0 on error.
   
   #category Internal
*/
size_t ser_assign_holder(ser_job_t * job, void * thing, size_t size)
{
  int n;

  if (!job || !thing)
    return 0;

  n = ser_find_holder(job, thing, size);

  if (n)
    return n;

  /* It isn't in the list. Make sure there's room to add it. */

  if (job->things >= job->allocated)
  {
    if (ser_job_realloc_holders(job, job->allocated * 2) == 0)
      return 0;
  }

  n = job->things;

  SER_DPRINT("assigning holder[%d].start %p\n", n, thing);

  job->holder[n].start = thing;
  job->things++;

  return n;
}


/**
   Returns the index of the holder in @job pointing to @thing@, or 0
   if it could not be found.

   See also: ser_assign_holder

   #category Internal
*/
size_t ser_find_holder(ser_job_t * job, void * thing, size_t size)
{
  int i;

  if (!job || !thing)
    return 0;

  for (i = 1; i < job->things; i++)
  {
    if (job->holder[i].start == thing)
    {
      if (size == 0 || size == job->holder[i].size)
	return i;
    }
  }

  return 0;
}


/**
   Serializes a primitive value and adds it to @job@s output
   buffer. @holder is the index of the value; @type is
   self-explanatory and @field_ptr is the start of the memory to be
   translated.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_write_primitive(ser_job_t * job, const size_t holder,
			const char * type, void * field_ptr)
{
  char tmp[SER_MSGLEN];

  if (!job || !type || !field_ptr)
    return false;

  snprintf(tmp, SER_MSGLEN, "%s #%ld%s",
	   type, holder,
	   (job->options & SER_OPT_COMPACT ? "{" : "\n{\n  "));
  ser_job_cat(job, tmp);

  ser_write_value(job, type, 0, field_ptr);

  ser_job_cat(job, (job->options & SER_OPT_COMPACT ? "}" : "\n}\n"));
  
  return true;
}



/**
   Converts @thing to human-readable text, according to the translator
   for @type present in @job. The result will be appended to the output
   buffer in @job.

   Returns true if @thing could be translated properly, false if
   something went wrong.
   
   #category Internal
*/
bool ser_dump(ser_job_t * job, char * type, void * thing)
{
  ser_tra_t * tra;
  size_t holder_index;
  char tmp[SER_MSGLEN];

  /* Find the translator for the thing */
  tra = ser_find_tra(job->first_tra, type);

  if (tra == NULL)
  {
    snprintf(tmp, SER_MSGLEN, "/* unknown type %s */\n", type);
    ser_job_cat(job, tmp);
    return false;
  }
  
  holder_index = ser_find_holder(job, thing, tra->size);

  if (holder_index == 0)
    return false;

  if (ser_write_struct(job, tra, holder_index, thing, 0) == 0)
    return false;

  return true;
}



/**
   Adds to @job@s output buffer a textual representation of the
   structure located at @thing@ serialized using @tra@.

   @ptr_index should be the index of the holder pointing to @thing@ or
   zero if @thing does not have any memory allocation of its own
   (e.g. it is an anonymous sub-structure).

   Returns true if everything went o-kay, false if something broke.
   
   #category Internal
*/
bool ser_write_struct(ser_job_t * job, ser_tra_t * tra,
		      size_t ptr_index, void * thing, const int anon)
{
  char tmp[SER_MSGLEN];
  size_t repeat;
  int compact;
  ser_field_t * field;
  void * array_start;

  SER_DPRINT("writing struct %s holder[%ld] @ %p\n", tra->id, ptr_index, thing);

  compact = job->options & SER_OPT_COMPACT;

  /*
    If this is an anonymous structure (embedded within another
    structure) we don't want to print the type and reference id
    (in fact, they shouldn't have any holder at all!).
  */
  if (!anon)
  {
    snprintf(tmp, SER_MSGLEN, "%s #%ld%s",
	     tra->id, ptr_index,
	     (compact ? "" : "\n"));
    ser_job_cat(job, tmp);
  }

  if (!anon)
    sprintf(tmp, "{%s", (compact ? "" : "\n"));
  else
    sprintf(tmp, "{%s", (compact ? "" : " "));

  ser_job_cat(job, tmp);


  for (field = tra->first_field; field; field = field->next)
  {
    /*
      Get the number of elements to write. For dynamic arrays this
      should have been filled in from the array count field. For
      null-terminated arrays we will calculate this now. For static
      arrays just use the field repeat count.
    */    
    if (tra->atype == ser_array_dynamic)
      repeat = job->holder[ptr_index].elements;
    else if (tra->atype == ser_array_nullterm)
      repeat = ser_nullterm_len(thing, field->type);
    else
      repeat = field->repeat;

    SER_DPRINT("%s %s, repeat %ld\n", field->type, field->tag, repeat);
    
    /*
      Don't write dynamic arrays if we don't know where they
      end. Don't write array count fields (they are useless; we will
      get that information from parsing the array anyway).
    */
    if (field->tag[0] == '@' || repeat == 0)
      continue;
    
    if (!compact && !anon)
      ser_job_cat(job, "  ");
    
    array_start = thing + field->offset;

    if (ser_write_array(job, tra, field, array_start, repeat) == 0)
      return false;

    if (!compact)
      ser_job_cat(job, (anon ? " " : "\n"));
  }

  if (tra->custom)
    tra->custom(job, tra, thing);
  
  ser_job_cat(job, (compact ? "}" : (anon ? "}" : "}\n") ));
  
  return true;
}



/**
   Sets the number of elements for holder @holder_index in @job to
   @elements@.
   
   Returns true on success, false on error (in which case
   @holder_index was probably outside the allocated range).

   #category Internal
*/
bool ser_set_holder_elements(ser_job_t * job, size_t holder_index, size_t elements)
{
  if (!job || !holder_index || holder_index > job->allocated)
    return false;
    
  job->holder[holder_index].elements = elements;

  return true;
}



/**
   Looks for a holder in @job that points to @start@ and a field in
   @tra that is the array count field of @field@ (same name, prepended
   by an at sign). If both are found the holder element count will be set to
   the array count field value. @thing is the structure referencing
   @start@; i.e. @thing has one or more pointers to @start that are
   also registered in the translator.

   Returns true if the field was found and the holder count set, false
   if it could not be found or an error occured.

   #category Internal
 */
bool ser_element_count(ser_job_t * job, ser_tra_t * tra,
			 ser_field_t * field, void * thing, void * start)
{
  ser_field_t * count_field;
  size_t holder_index;
  size_t c;

  holder_index = ser_find_holder(job, *(void**)start, 0);
  
  if (holder_index == 0)
    return false;

  count_field = ser_find_array_count(tra, field->tag);
  
  if (count_field == 0)
  {
    SER_DPRINT("no count field found for %s\n", field->tag);
    return false;
  }

  c = ser_numeric_cast(thing + count_field->offset, count_field->type);

  ser_set_holder_elements(job, holder_index, c);
  
  SER_DPRINT("using count field %s = %ld\n", count_field->tag, c);

  return true;
}



/**
   Writes @elements fields, starting at @array_start@, to @job@s
   output buffer. @tra should be the type of container structure and
   @field the type of output.

   Returns true if the array could be written, false on error.

   #category Internal
*/
bool ser_write_array(ser_job_t * job, ser_tra_t * tra, ser_field_t * field,
		     void * array_start, size_t elements)
{
  char tmp[SER_MSGLEN];
  size_t i;
  void * field_ptr;

  if (!job || !field || !array_start)
    return false;

  SER_DPRINT("writing array: %s:%s * %ld, start %p\n",
	     tra->id, field->type, elements, array_start);
 
  if (tra->atype == ser_flat)
  {
    snprintf(tmp, SER_MSGLEN, "%s ", field->tag);
    ser_job_cat(job, tmp);
    
    if (elements != 1)
      ser_job_cat(job, "{");
  }
    
  for (i = 0; i < elements; i++)
  {
    field_ptr = array_start + ser_field_size(job, field->type, field->ref) * i;
    
    SER_DPRINT("-- array index %ld\n", i);

    if (ser_write_value(job, field->type, field->ref, field_ptr) == 0)
      return false;
 
    if (elements != 1 && i < elements - 1)
      ser_job_cat(job, (job->options & SER_OPT_COMPACT ? "," : ", "));
  }

  if (tra->atype == ser_flat)
  {
    if (elements != 1)
      ser_job_cat(job, "}");
    
    ser_job_cat(job, ";");
  }
  
  return true;
}


/**
   Sets the custom output handler for @tra to @func_ptr@. There can
   only be one such function per translator; if multiple calls are
   needed (for OO-like functionality) a wrapper function is needed.

   #category Internal
*/
void ser_set_custom_handler(ser_tra_t * tra,
			    int (*func_ptr)(ser_job_t *, ser_tra_t *, void *))
{
  if (!tra || !func_ptr)
    return;

  tra->custom = func_ptr;

  return;
}


/**
   Attempts to convert the data at @ptr into a @type number. No type
   or overflow checking is performed.

   Returns the number (0 on error).

   #category Internal
*/
long ser_numeric_cast(void * ptr, char * type)
{
  if (!ptr || !type)
    return 0;

  if (strcmp(type, "char") == 0)
    return *(char*)ptr;
  else if (strcmp(type, "short") == 0)
    return *(short*)ptr;
  else if (strcmp(type, "ushort") == 0)
    return *(unsigned short*)ptr;
  else if (strcmp(type, "int") == 0)
    return *(int*)ptr;
  else if (strcmp(type, "uint") == 0)
    return *(unsigned int*)ptr;
  else if (strcmp(type, "long") == 0)
    return *(long*)ptr;
  else if (strcmp(type, "ulong") == 0)
    return *(unsigned long*)ptr;
  else if (strcmp(type, "int8_t") == 0)
    return *(int8_t*)ptr;
  else if (strcmp(type, "uint8_t") == 0)
    return *(uint8_t*)ptr;
  else if (strcmp(type, "int16_t") == 0)
    return *(int16_t*)ptr;
  else if (strcmp(type, "uint16_t") == 0)
    return *(uint16_t*)ptr;
  else if (strcmp(type, "int32_t") == 0)
    return *(int8_t*)ptr;
  else if (strcmp(type, "uint32_t") == 0)
    return *(int8_t*)ptr;
  else if (strcmp(type, "intptr_t") == 0)
    return *(intptr_t*)ptr;
  else if (strcmp(type, "size_t") == 0)
    return *(size_t*)ptr;
  else if (strcmp(type, "time_t") == 0)
    return *(time_t*)ptr;

  SER_DPRINT("warning: unknown cast \"%s\"; returning 0\n", type);

  return 0;
}



/**
   Returns a pointer to any field in @tra that is an array count for
   @array_tag; i.e. one that is @array_tag prepended with a @.

   #category Internal
*/
ser_field_t * ser_find_array_count(ser_tra_t * tra, char * array_tag)
{
  ser_field_t * f;

  if (!tra || !array_tag)
    return NULL;

  for (f = tra->first_field; f; f = f->next)
  {
    if (f->tag[0] == '@' && strcmp(f->tag + 1, array_tag) == 0)
      return f;
  }

  return NULL;
}



/**
   Searches @thing for reference fields and adds their destination to
   @job@s list of holders. If @holder_index is non-zero the type
   specified for that holder will be used as translator, otherwise
   @thing_tra must be provided.

   Returns true on success, false on utter, catastrophic failure.

   #category Internal
*/
bool ser_follow_ptrs(ser_job_t * job, size_t holder_index,
		     ser_tra_t * thing_tra, void * thing)
{
  char tmp[SER_MSGLEN];
  ser_field_t * field;
  void * field_ptr;
  void * array_start;
  char * type;
  size_t i;
  size_t repeat;
  size_t el_size;

  if (!job)
    return false;

  if (holder_index)
  {
    if (holder_index > job->allocated)
      return false;

    type = job->holder[holder_index].type;

    if (ser_is_primitive(type))
    {
      SER_DPRINT("holder[%ld] is primitive (%s); skipping\n", holder_index, type);
      return true;
    }
    
    thing_tra = ser_find_tra(job->first_tra, type);
    thing = job->holder[holder_index].start;
  }

  if (!thing || !thing_tra)
    return false;
  
  if (holder_index)
    SER_DPRINT("using holder %ld; ", holder_index);

  SER_DPRINT("following pointers in %s at %p\n", thing_tra->id, thing);

  /*
    Scan each field in the translator.
  */
  for (field = thing_tra->first_field; field; field = field->next)
  {
    el_size = ser_field_size(job, field->type, field->ref);
    array_start = thing + field->offset;

    SER_DPRINT("-- field %s %s: ", field->type, field->tag);

    /* These are of no relevance at this point. */
    if (field->tag[0] == '@')
    {
      SER_DPRINT("array count, skipping\n");
      continue;
    }

    if (thing_tra->atype == ser_array_dynamic ||
	thing_tra->atype == ser_array_nullterm)
    {
      /*
	If we get a dynamic array we must use its element count to
	cover everything. This might be an array of pointers or an
	array of structures containing pointers. This should only
	process a single field.
      */
      if (field->next)
      {
	SER_DPRINT("%s is dynamic array but has more than one field\n", thing_tra->id);
	break;
      }

      SER_DPRINT("using repeat %ld\n", repeat);
      repeat = job->holder[holder_index].elements;
    }
    else if (thing_tra->atype == ser_flat)
    {
      /* It's just a regular structure. */
      repeat = field->repeat;
    }

    if (!field->ref && !ser_is_primitive(field->type))
    {
      /*
	It's an anonymous structure within thing. We won't add this
	one to the holder list since it shares memory with the parent,
	but we will recursively scan it for more pointers.
      */

      SER_DPRINT("anonymous struct\n");

      ser_tra_t * anon_tra;
      anon_tra = ser_find_tra(job->first_tra, field->type);
      
      if (!anon_tra && !ser_is_primitive(field->type))
      {
	snprintf(tmp, SER_MSGLEN,
		 "warning: no translator for type %s", field->type);
	job->log_func(tmp);
	return false;
      }

      SER_DPRINT("-- -- using translator %s (size %ld)\n", anon_tra->id, el_size);
      
      /* Descend into the anonymous structure and see if it has any references. */
      for (i = 0; i < repeat; i++)
      {
	field_ptr = array_start + el_size * i;
	ser_follow_ptrs(job, 0, anon_tra, field_ptr);
      }
	
      continue;
    }
    else if (field->ref)
    {
      /*
	This field is a pointer.
      */

      SER_DPRINT("reference\n");

      size_t target_index;
      size_t target_size;

      for (i = 0; i < repeat; i++)
      {
	field_ptr = array_start + el_size * i;
	
	if (*(void **)field_ptr == NULL)
	{
	  SER_DPRINT("NULL pointer; skipping...\n");
	  continue;
	}
	
	/*
	  Find out how large the size of the destination area needs to
	  be. We need to know this so ser_assign_holder can keep
	  pointers of different size apart and so ser_ptr_overlap can
	  perform overlap checking. field_size has 0 for reference
	  so get the size of the target, not the pointer itself.
	*/
	target_size = ser_field_size(job, field->type, 0);

	/* Put the pointer destination in a holder */
	target_index = ser_assign_holder(job, *(void **)field_ptr, target_size);
	
	if (!target_index)
	{
	out_of_memory:
	  snprintf(tmp, SER_MSGLEN,
		   "error: out of memory");
	  job->log_func(tmp);
	  return false;
	}

	/*
	  Register what the structure is.
	*/
	job->holder[target_index].size = target_size;

	job->holder[target_index].type = strdup(field->type);

	if (!job->holder[target_index].type)
	  goto out_of_memory;

	/*
	  Finally, find out how _many_ thing_tra
	  are supposed to be at thing.
	*/
	ser_element_count(job, thing_tra, field, thing, field_ptr);

	SER_DPRINT("holder[%ld]: %s is %s (size %ld) ref to %p\n",
		   target_index, field->tag, field->type, target_size, thing);
      }

      continue;
    }

    SER_DPRINT("nothing to do\n");
  }
  
  return true;
}



/*
  WIP

  #category Internal
*/
bool ser_make_reference(ser_job_t * job, const void * thing,
			const size_t size, const size_t skip,
			char ** dest)
{
  size_t hi;
  char * tag;
  size_t ai;
  size_t hsz;
  size_t fsz;
  uintptr_t pos;
  uintptr_t start;
  uintptr_t elpos;
  ser_tra_t * tra;
  ser_field_t * field;
  ser_holder_t * holder;

  char * ret;

   if (!job || !thing)
    return false;

  pos = (uintptr_t)thing;

  for (hi = 1; hi < job->allocated; hi++)
  {
    if (hi == skip)
      continue;

    holder = &job->holder[hi];
    hsz = holder->size;
    start = (uintptr_t)holder->start;

    if (pos < start || pos >= start + hsz)
      continue;

    /* thing points within this structure. */

    /* Does it point to the structure itself? */
    if (size == hsz && pos == start)
    {
      tag = NULL;
      ai = 0;
      goto found_it;
    }
    
    /* Check each field. */
    tra = ser_find_tra(job->first_tra, holder->type);

    if (!tra)
      continue;

    for (field = tra->first_field; field != NULL; field = field->next)
    {
      fsz = ser_field_size(job, field->type, field->ref);
      
      if (fsz != size)
	continue;

      for (ai = 0; ai < field->repeat; ai++)
      {
	elpos = start + field->offset + (ai * fsz);

	if (elpos == pos)
	{
	  tag = field->tag;
	  goto found_it;
	}
      }
    }
  }

  /* Didn't find it */
  return false;

found_it:

  if (dest == NULL)
    return true;

  char * p;

  ret = malloc(1000);
  sprintf(ret, "#%ld", hi);
  
  p = ret;
  while (*p) p++;

  if (tag)
  {
    *p++ = '.';
    *p = '\0';
    strcat(p, tag);

    if (ai)
    {
      while (*p) p++;
      sprintf(p, "[%ld]", ai);
    }
  }

  *dest = ret;

  return true;
}




/**
   Appends to @job@s output buffer a textual representation of @field_ptr
   as a @type. If the field is a reference, it will also register in
   @job@s "holder" list what type of translation to use for the
   destination structure in the future.

   Returns true if the value could be written properly, false on error.

   #category Internal
*/
bool ser_write_value(ser_job_t * job, const char * type,
		     const int ref, void * field_ptr)
{
  char tmp[200];

  tmp[0] = '\0';

  SER_DPRINT("ref: %d\n", ref);

  if (!job || !type || !field_ptr)
    return false;

  if (strcmp(type, "string") == 0)
  {
    if (*(char **)field_ptr == NULL)
    {
      ser_job_cat(job, "NULL");
    }
    else
    {
      char * esc;
      esc = ser_escape_str(*(char **)field_ptr);
      ser_job_cat(job, "\"");
      ser_job_cat(job, esc);
      ser_job_cat(job, "\"");
      free(esc);
    }
    return 1;
  }
  else if (ref == 1)
  {
    if (*(void**)field_ptr == NULL)
    {
      sprintf(tmp, "NULL");
    }
    else
    {
      char * refstr;
      bool r;
      
      r = ser_make_reference(job, *(void **)field_ptr,
			     ser_field_size(job, type, 0), 0, &refstr);
      
      if (!r)
      {
	ser_list_holders(job);

	sprintf(tmp, "error: missing reference for %s", type);
	job->log_func(tmp);
	return false;
      }

      sprintf(tmp, refstr);
      free(refstr);
    }
  }
  else if (strcmp(type, "bool") == 0)
  {
    if (*(bool*)field_ptr)
      strcpy(tmp, "true");
    else
      strcpy(tmp, "false");
  }
  else if (strcmp(type, "short") == 0)
    sprintf(tmp, "%d", *(short *)field_ptr);
  else if (strcmp(type, "ushort") == 0)
    sprintf(tmp, "%u", *(ushort *)field_ptr);
  else if (strcmp(type, "int") == 0)
    sprintf(tmp, "%d", *(int *)field_ptr);
  else if (strcmp(type, "uint") == 0)
    sprintf(tmp, "%u", *(unsigned int *)field_ptr);
  else if (strcmp(type, "long") == 0)
    sprintf(tmp, "%ld", *(long *)field_ptr);
  else if (strcmp(type, "ulong") == 0)
    sprintf(tmp, "%lu", *(unsigned long *)field_ptr);
  else if (strcmp(type, "uint8_t") == 0)
    sprintf(tmp, "%d", *(uint8_t *)field_ptr);
  else if (strcmp(type, "uint16_t") == 0)
    sprintf(tmp, "%d", *(uint16_t *)field_ptr);
  else if (strcmp(type, "uint32_t") == 0)
    sprintf(tmp, "%d", *(uint32_t *)field_ptr);
  else if (strcmp(type, "int8_t") == 0)
    sprintf(tmp, "%d", *(int8_t *)field_ptr);
  else if (strcmp(type, "int16_t") == 0)
    sprintf(tmp, "%d", *(int16_t *)field_ptr);
  else if (strcmp(type, "int32_t") == 0)
    sprintf(tmp, "%d", *(int32_t *)field_ptr);
  else if (strcmp(type, "intptr_t") == 0)
    sprintf(tmp, "%ld", *(intptr_t *)field_ptr);
  else if (strcmp(type, "time_t") == 0)
    sprintf(tmp, "%ld", *(time_t *)field_ptr);
  else if (strcmp(type, "size_t") == 0)
    sprintf(tmp, "%lu", *(size_t *)field_ptr);
  else if (strcmp(type, "float") == 0)
    sprintf(tmp, "%e", *(float *)field_ptr);
  else if (strcmp(type, "double") == 0)
    sprintf(tmp, "%le", *(double *)field_ptr);
  else if (strcmp(type, "ldouble") == 0)
    sprintf(tmp, "%Le", *(long double *)field_ptr);
  else if (strcmp(type, "char") == 0)
  {
    int c;
    
    c = (*(char *)field_ptr) % 256;
    
    /* Escape non-printable characters */
    if (isprint(c))
      sprintf(tmp, "'%c'", c);
    else
      sprintf(tmp, "'\\x%02X'", c);
  }
  else if (ref == 0)
  {
    ser_tra_t * anon_tra;

    anon_tra = ser_find_tra(job->first_tra, type);

    if (!anon_tra)
    {
      sprintf(tmp, "warning: no translator for type %s", type);
      job->log_func(tmp);
      return false;
    }
    else
    {
      ser_write_struct(job, anon_tra, 0, field_ptr, 1);
    }
  }
  else
  {
    sprintf(tmp, "/* error: bad field %s */", type);
    ser_job_cat(job, tmp);

    sprintf(tmp, "error: bad field %s", type);
    job->log_func(tmp);
    return 0;
  }

  ser_job_cat(job, tmp);

  return true;
}




/**
   Resizes the holder list of @job to @n elements. If the list is
   expanded, new entries will be cleared.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_job_realloc_holders(ser_job_t * job, const int n)
{
  ser_holder_t * new_holder;
  int old_n;
  int i;

  if (!job)
    return false;

  old_n = job->allocated;
  job->allocated = n;

  SER_DPRINT("allocating %d holders for job\n", n);

  new_holder = realloc(job->holder, sizeof(ser_holder_t) * n);

  if (!new_holder)
    return false;

  job->holder = new_holder;

  /* Clear the resized array */
  for (i = old_n; i < job->allocated; i++)
  {
    SER_DPRINT("clearing holder #%d\n", i);
    ser_clear_holder(&job->holder[i]);
  }

  return true;
}



/**
   Clears @holder@. This will not free any pointers associated with
   it, only reset them to neutral values.

   #category Internal
*/
void ser_clear_holder(ser_holder_t * holder)
{
  if (!holder)
    return;

  holder->start = NULL;
  holder->type = NULL;
  holder->elements = 0;
  holder->replace_id = 0;
  holder->replace_field = NULL;
  holder->size = 0;

  return;
}



/**
   Default error logger. Implement your own and pass it to ser_ialize()
   and ser_parse() to redirect errors to your custom logging console,
   message boxes, etc.

   #category Logging
*/
void ser_def_log(char * msg)
{
  fprintf(stderr, "%s\n", msg);

  return;
}


/**
   Discards messages. This is where "muted" output ends up.

   #category Logging
*/
void ser_null_log(char * msg)
{
  return;
}


/**
   Creates a copy of @s suitable for ser_parse. Semicolons, commas,
   braces outside quotes will be padded with spaces.

   Returns the padded string or NULL on error.

   #category Strings
*/
char * ser_preformat(char * s)
{
  char * pos;
  char * ret;
  char * d;

  int count;
  int quote;
  int comment;
  
  if (!s)
    return NULL;

  count = 0;
  quote = 0;
  comment = 0;

  pos = s;

  while (*pos != '\0')
  {
    if (comment)
    {
      if (*pos == '*' && *(pos + 1) == '/')
      {
	pos += 2;
	count += 3;
	comment = 0;
      }
      else
      {
	pos++;
	count++;
      }
    }
    else if (*pos == '\"')
    {
      /*
	If we encounter a quote character without a \ in front,
	flip the quote flag to keep track we're in a string.
      */
      count++;
      pos++;
      quote = !quote;
    }
    else if (!quote && *pos == '/' && *(pos + 1) == '*')
    {
      comment = 1;
      count += 3;
      pos += 2;
    }
    else if (quote && *pos == '\\')
    {
      /*
	Quotes after a slash are escaped, but only
	if they themselves are within quotes.
      */
      count++;
      pos++;
    
      if (*pos == '\"' || *pos == '\\')
      {
	count++;
	pos++;
      }
    }
    else if ((*pos == ';' || *pos == ',' || *pos == '{' || *pos == '}') && !quote)
    {
      /*
	Add two extra bytes for spaces around
	terminators, separators and braces.
      */
      count += 3;
      pos++;
    }
    else
    {
      count++;
      pos++;
    }
  }

  count++; /* Termination character */

  SER_DPRINT("ser_preformat(): original %d, new %d\n", (int)strlen(s), count);

  ret = malloc(count);

  if (ret == NULL)
    return NULL;

  quote = 0;
  d = ret;
  pos = s;

  /*  */
  while (*pos != '\0')
  {
    if (comment)
    {
      if (*pos == '*' && *(pos + 1) == '/')
      {
	*d++ = *pos++;
	*d++ = *pos++;
	*d++ = ' ';

	comment = 0;
      }
      else
      {
	*d++ = *pos++;
      }
    }
    else if (*pos == '\"')
    {
      quote = !quote;
      *d++ = *pos++;
    }
    else if (!quote && *pos == '/' && *(pos + 1) == '*')
    {
      *d++ = ' ';
      *d++ = *pos++;
      *d++ = *pos++;
    }
    else if (*pos == '\\' && quote)
    {
      *d++ = *pos++;
      
      if (*pos == '\"' || *pos == '\\')
      {
	*d++ = *pos++;
      }
    }
    else if ((*pos == ';' || *pos == ',' || *pos == '{' || *pos == '}') && !quote)
    {
      *d++ = ' ';
      *d++ = *pos;
      *d++ = ' ';
      pos++;
    }
    else
    {
      *d++ = *pos++;
    }
  }

  /* Terminate the new string */
  *d = '\0';

  return ret;
}



/**
   Parses (inflates) @s according to the translator list
   @first_tra. @expected_type should specify the type to expect for
   index #1; if it differs the process will fail.
   
   If @log_func is provided it will be called with error messages
   if inflation fails. If it is NULL output will be muted.
   See ser_def_log().
   
   Returns a pointer to element #1 (correspondingly passed as @first to
   ser_ialize()) or NULL on failure.

   #category Serialization
*/
void * ser_parse(ser_tra_t * first_tra, const char * expected_type,
		 char * s, void (*log_func)(char*))
{
  ser_job_t job;
  char line[SER_MSGLEN];

  void * first;
  char * type;
  char * tok;
  ser_tok_t r;
  ser_tra_t * tra;
  long int tmp;
  long int id;

  first = NULL;

  if (/*!first_tra ||*/ !s)
    return NULL;

  job.first_tra = first_tra;

  if (log_func)
    job.log_func = log_func;
  else
    job.log_func = &ser_null_log;
  
  SER_DPRINT("parse: start parsing\n");

  job.first_sptr = NULL;
  job.holder = NULL;
  job.things = 0;
  job.allocated = 0;

  /*
    We always want to allocate at least 1 holder. If there is no input
    to parse, we will still return index 1 (that should be NULL).
  */
  ser_job_realloc_holders(&job, 1);
  
  job.p_str = ser_preformat(s);
  
  if (job.p_str == NULL)
    return NULL;
  
  job.p_pos = job.p_str;
  job.p_end = strchr(job.p_str, '\0');
  
  do
  {
    /* First we would like to know what kind of type we should inflate. */
    
    SER_DPRINT("parse: expecting type (or end of input)...\n");
  
    r = ser_ntok(&job, &type, &tmp);

    if (r == ser_tok_end)
    {
      /* We have reached the end of the input. No more need for parsing. */
      goto count_stuff;
    }
    else if (r != ser_tok_any)
    {
      /* We received something that clearly isn't a type. */
      snprintf(line, SER_MSGLEN, "error: expecting type at byte %ld, got %s",
	       job.p_byte, ser_token_code(r));
      job.log_func(line);
      goto cleanup;
    }

    SER_DPRINT("parse: expecting index...\n");

    r = ser_ntok(&job, &tok, &tmp);

    if (r == ser_tok_err)
      goto cleanup;
    else if (r != ser_tok_id)
    {
      /* We will only accept an index reference at this point. */
      snprintf(line, SER_MSGLEN, "error: expecting index at byte %ld, got %s",
	       job.p_byte, ser_token_code(r));
      job.log_func(line);
      goto cleanup;
    }

    /* Store the numeric index; this is the structure we will work on. */
    id = tmp;

    SER_DPRINT("parse: expecting opening brace or redirection...\n");

    r = ser_ntok(&job, &tok, &tmp);

    if (r == ser_tok_err)
      goto cleanup;
    else if (r == ser_tok_open)
    {
      /*
	Make sure the holder list is big enough. Remember that
	job.allocated doesn't include index 0.
      */
      if (id >= job.allocated)
	ser_job_realloc_holders(&job, id + 1);
      
      /* Is it a duplicate? */
      if (job.holder[id].start != NULL)
      {
	snprintf(line, SER_MSGLEN, "error: index %ld already assigned", id);
	job.log_func(line);
	return 0;
      }

      if (r == ser_tok_open)
      {
	/* We need to keep this around to preserve type safety for references.*/
	job.holder[id].type = strdup(type);

	if (ser_is_primitive(type))
	{
	  if (ser_read_primitive(&job, id, type) == 0)
	    goto cleanup;
	}
	else
	{
	  /* Find the translator for this type. */
	  tra = ser_find_tra(first_tra, type);
	  
	  if (tra == NULL)
	  {
	    snprintf(line, SER_MSGLEN, "error: missing translator for \"%s\"", type);
	    job.log_func(line);
	    goto cleanup;
	  }
	  
	  if (ser_read_struct(&job, id, tra) == 0)
	    goto cleanup;
	}
      }
    }
    else
    {
      snprintf(line, SER_MSGLEN,
	       "error: expecting opening brace at byte %ld, got %s",
	       job.p_byte, ser_token_code(r));
      job.log_func(line);
      goto cleanup;
    }

    continue;
  } while (r != ser_tok_err);

count_stuff:

  if (strcmp(expected_type, job.holder[1].type))
  {
    /*
      The data read for #1 doesn't match the type we're
      expecting. first is already NULL. Abort.
    */
    snprintf(line, SER_MSGLEN,
	     "error: %s #1 does not match expected type %s",
	     job.holder[1].type, expected_type);
    job.log_func(line);
    
    goto cleanup;
  }

  if (ser_restore_pointers(&job) == false)
    goto cleanup;

  first = job.holder[1].start;

cleanup:

  ser_del_subst_ptrs(&job);

  if (job.allocated)
  {
    size_t i;

    for (i = 1; i < job.allocated; i++)
      free(job.holder[i].type);
  }

  free(job.holder);

  free(job.p_str);
  
  return first;
}



/**
   Reads a primitive @type from @job@s parse buffer. The result will be
   stored in holder @id@ after allocating sufficient memory to stort
   the type.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_read_primitive(ser_job_t * job, const long id, const char * type)
{
  ser_holder_t * holder;

  SER_DPRINT("ser_read_primitive\n");

  if (!job || !id || !type)
    return false;

  holder = &job->holder[id];

  if (!holder)
    return false;

  holder->type = strdup(type);

  /*
    This doesn't get a job; it shouldn't need one since we're after a
    primitive type.
  */
  holder->start = malloc(ser_field_size(NULL, type, 0));

  if (!holder->start || !holder->type)
    return false;

  ser_tok_t r;
  char * value;
  long tmp;

  r = ser_ntok(job, &value, &tmp);

  if (ser_read_value(job, type, r, holder->start, value, tmp) == 0)
    return false;

  return ser_expect(job, ser_tok_close);
}



/**
   Returns a pointer to the start of field @tag (array offset @ai@) in
   the structure held by holder index @hi@ in @job@. Returns NULL if
   it cannot be found.

   If @type is provided the pointer at that location will be pointed
   to a string with the type of the destination. This is in use
   somewhere else in the program and should not be freed.

   If @ref is provided it will be set to true if the resolved address
   points to a reference field or false if it is a normal value or the
   beginning of a struct (possibly anonymous).

   #category Internal
*/
void * ser_resolve_subst_ptr(ser_job_t * job, const size_t hi,
			     const char * tag, const size_t ai,
			     char ** type, bool * ref)
{
  void * ret;
  ser_holder_t * holder;
  ser_field_t * field;
  
  if (type)
    *type = NULL;
  
  if (!job)
    return NULL;

  if (hi > job->allocated)
    return NULL;

  holder = &job->holder[hi];

  if (!holder)
    return NULL;

  ret = holder->start;

  if (type)
    *type = job->holder[hi].type;

  if (ref)
    *ref = false;

  if (tag)
  {
    field = ser_find_field(job->first_tra, holder->type, tag);
    
    if (!field)
      return NULL;

    if (ai >= field->repeat)
      return NULL;

    ret += field->offset +
      (ai * ser_field_size(job, field->type, field->ref));

    if (type)
      *type = field->type;

    if (ref && field->ref)
      *ref = true;
  }

  return ret;
}



/**
   Checks that all substitution pointers in @job resolve to valid
   locations (the holder, field and array index (if >0) must exist).

   #category Internal
 */
bool ser_check_subst_ptrs(ser_job_t * job)
{
  char line[SER_MSGLEN];
  ser_subst_ptr_t * sptr;
  void * new_ptr;

  if (!job)
    return false;

  SER_DPRINT("ser_subst_ptrs()\n");

  for (sptr = job->first_sptr; sptr != NULL; sptr = sptr->next)
  {
    SER_DPRINT("subst ptr: holder[%ld].%s[%ld]\n",
	       sptr->d_hi,
	       (sptr->d_tag ? sptr->d_tag : "(base)"),
	       sptr->d_ai);
    
    new_ptr = ser_resolve_subst_ptr(job, sptr->d_hi, sptr->d_tag,
				    sptr->d_ai, NULL, NULL);
    
    if (!new_ptr)
    {
      snprintf(line, SER_MSGLEN,
	       "error: bad reference %ld",
	       sptr->d_hi);
      job->log_func(line);
      return false;
    }
    
    SER_DPRINT("-- should replace with %p\n", new_ptr);
  }

  return true;
}



/**
   Creates a new substitution pointer structure in @job@. When
   dereferenced, the subst pointer will resolve to
   holder[@d_hi@].@d_tag@[@d_ai@]. Any pointer that wants to use this
   location should point themselves at the structure returned.

   #category Internal
*/
ser_subst_ptr_t * ser_add_subst_ptr(ser_job_t * job, const size_t d_hi,
				    const char * d_tag, const size_t d_ai)
{
  ser_subst_ptr_t * sptr;

  if (!job)
    return NULL;
    
  sptr = malloc(sizeof(ser_subst_ptr_t));

  if (!sptr)
    return NULL;

  sptr->d_hi = d_hi;
  sptr->d_tag = NULL;
  sptr->d_ai = d_ai;

  if (d_tag)
  {
    sptr->d_tag = strdup(d_tag);

    if (!sptr->d_tag)
    {
      free(sptr);
      return NULL;
    }
  }

  sptr->next = job->first_sptr;
  job->first_sptr = sptr;

  return sptr;
}



/*
  Picks @token apart and returns its components. @token might be
  modified and must therefore be writeable.

  @token should be a reference of the form #holder.field[a] where
  holder is the numeric index of a holder, field the tag och a field
  name within that holder and a the array offset for that field. [a]
  may be omitted, .field may be omitted if no [a] is provided.

  The result will be stored at the locations pointed to by
  @holder_dest, @field_dest and @index_dest@. If @token contains no
  field tag, *field_dest will point at NULL. If @token contains no
  array index *index_dest will be zero. All three destinations must be
  provided even if no value is expected for them. The string at
  *field_dest will be a substring of @token and should not be freed
  separately.

  Returns false on error (token wasn't a reference), true on success;
  note however that this doesn't say if the reference is valid (holder
  out of bounds, field tag or array index that doesn't exist), just
  that the expression could be parsed.
*/
bool ser_dissect_reference(char * token, size_t * holder_dest,
			   char ** field_dest, size_t * index_dest)
{
  size_t holder;
  size_t index;
  char * dot;
  char * bracket_start;
  char * bracket;
  char * bracket_end;
  char * read_end;
  char * token_end;

  if (!token || !holder_dest || !field_dest || !index_dest)
    return false;

  /* If no array index is found we will assume 0. */
  index = 0;

  /* Find the end of the token string */
  token_end = token;
  while (*token_end)
    token_end++;

  /* Find an opening bracket. */
  bracket_start = strchr(token, '[');
  
  if (bracket_start)
  {
    bracket = bracket_start;
    *bracket = '\0';
    bracket++;

    /* Extract the array index. */
    if (*bracket == '\0')
      return false;

    index = strtoul(bracket, &bracket_end, 10);
    
    /* Malformed number or unterminated bracket? */
    if (*bracket_end != ']' || bracket_end == bracket)
      return false;
  }

  /*
    Find the dot. A field tag should follow; if there is none we will
    use the base address of the structure.
  */
  dot = strchr(token, '.');

  /* Check for "#." and "#[" */
  if (dot == (token + 1) || bracket_start == (token + 1))
    return false;

  /* Extract the holder index. +1 to skip the initial # in the id. */
  holder = strtoul(token + 1, &read_end, 10);

  /* Make sure the index ends where we expect it to. */
  if ((dot && read_end != dot) ||
      (!dot && bracket_start && read_end != bracket_start) ||
      (!dot && !bracket_start && read_end != token_end))
    return false;

  /* We got through that ok; set the destination values. */

  if (!dot || dot == token_end)
    *field_dest = NULL;
  else
  {
    if (*(dot + 1) == '\0')
      *field_dest = NULL;
    else
      *field_dest = dot + 1;
  }
  
  *holder_dest = holder;

  *index_dest = index;

  return true;
}



/**
   Goes through every structure held by @job and resolves reference
   indices to real pointers.

   For example, any reference to #2 will be replaced by the address in
   holder[2]->start.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_restore_pointers(ser_job_t * job)
{
  ser_holder_t * holder;
  ser_field_t * field;
  ser_tra_t * tra;
  size_t i;

  if (!job)
    return false;

  SER_DPRINT("job uses %ld holders\n", job->allocated);

  for (i = 1; i < job->allocated; i++)
  {
    if (job->holder[i].start == NULL &&
	(job->holder[i].replace_id == 0 ||
	 job->holder[i].replace_field == NULL))
    {
      SER_DPRINT("warning: holder[%ld] is null and lacks replace id/field", i);
    }
  }

  /*
    Try to convert reference indices to real pointers.  Scan each
    structure for reference fields, check if their index exists in
    the holder list.
  */

  for (i = 1; i < job->allocated; i++)
  {
    holder = &job->holder[i];

    if (holder == NULL ||
	holder->start == NULL)
    {
      SER_DPRINT("holder[%ld] is NULL\n", i);
      continue;
    }

    if (ser_is_primitive(holder->type))
    {
      SER_DPRINT("holder[%ld] is primitive type; nothing to restore\n", i);
      continue;
    }

    tra = ser_find_tra(job->first_tra, holder->type);
    
    SER_DPRINT("holder[%ld]: %s at %p%s\n",
	       i, tra->id, (void*)&job->holder[i],
	       (tra->atype ? " (array)" : ""));

    /*
      Fill in array count fields. This must be done before restoring
      pointers in the structure, or otherwise the holder indices (where
      the element count for dynamic arrays are stored) will already have
      been replaced with real pointers.
    */
    ser_restore_array_counts(job, holder, tra);

    /* Restore pointers */
    
    for (field = tra->first_field; field != NULL; field = field->next)
    {
      if (field->tag[0] == '@')
      {
	SER_DPRINT("field %s %s - array count, already dealt with\n",
		   field->type, field->tag);
      }
      else if (field->ref)
      {
	/* Try to restore pointer */
	SER_DPRINT("holder[%ld]: field %s %s - restoring pointer(s)...\n",
		   i, field->type, field->tag);

	if (ser_restore_ptr(job, holder, tra, field) == false)
	  return false;
      }
      else if (ser_is_primitive(field->type))
      {
	/* Do nothing */
	SER_DPRINT("field %s %s - native\n",
		   field->type, field->tag);
      }
    }
  }

  if (ser_check_subst_ptrs(job) == 0)
    return false;
  
  return true;
}



/**
   Resolves the destination holder for a substitution pointer structure
   located at @field in @holder. @tra should be the translator for
   @holder@. If @tra or @field is an array all elements will be
   processed.
   
   Returns true on success, false on failure.
   
   #category Internal
*/
bool ser_restore_ptr(ser_job_t * job, ser_holder_t * holder,
		     ser_tra_t * tra, ser_field_t * field)
{
  char line[SER_MSGLEN];

  ser_subst_ptr_t * sptr;
  size_t e_size; /* array element size */
  size_t repeat; /* number of array elements */
  size_t a_i; /* array index */
  void * field_base;
  void * subst_location;
      
  if (!job || !holder || !tra || !field)
    return false;

  e_size = ser_field_size(job, field->type, field->ref);
  
  field_base = holder->start + field->offset;
  
  /* */
  if (tra->atype == ser_array_dynamic ||
      tra->atype == ser_array_nullterm)
  {
    repeat = holder->elements;
    
    SER_DPRINT("array repeat %ld\n", repeat);
  }
  else
  {
    repeat = field->repeat;
    
    SER_DPRINT("single field or static array at offset %ld (repeat %ld)\n",
	       field->offset, repeat);
  }
  
  for (a_i = 0; a_i < repeat; a_i++)
  {
    subst_location = field_base + (e_size * a_i);
    
    sptr = *(ser_subst_ptr_t **)subst_location;
    
    /*printf("%ld, %ld\n\n", job->things, sptr->d_hi);
      printf("%s\n\n", job->holder[sptr->d_hi].type);*/

    if (sptr)
    {
      void * dest;
      char * type;
      bool ref;

      dest = ser_resolve_subst_ptr(job, sptr->d_hi, sptr->d_tag,
				   sptr->d_ai, &type, &ref);
      
      if (sptr->d_hi >= job->allocated || !dest)
      {
	snprintf(line, SER_MSGLEN,
		 "error: unable to resolve substitution pointer "
		 "for holder[%ld].%s[%ld]",
		 sptr->d_hi,
		 (sptr->d_tag ? sptr->d_tag : ""),
		 sptr->d_ai);
	job->log_func(line);
	return false;
      }

      if (strcmp(field->type, type) || ref)
      {
	/* The holder index is valid but types mismatch. */
	
	snprintf(line, SER_MSGLEN,
		 "error: type mismatch; expecting %s but destination is %s%s",
		 field->type,
		 type, (ref ? " reference" : ""));
	job->log_func(line);
	return false;
      }
      
      *(void **)(subst_location) = dest;
    }
  }

  return true;
}




/*
  Fill in array count fields for @holder@ (of type @tra@) in
  @job@. This will search for fields prefixed with @@ and a matching
  array pointer without the @@. The count field will be set to
*/
bool ser_restore_array_counts(ser_job_t * job, ser_holder_t * holder, ser_tra_t * tra)
{
  ser_subst_ptr_t * sptr;
  ser_holder_t * subst_holder;
  ser_field_t * field;
  ser_field_t * array_field;
  size_t array_target;
  char * array_name;
  void * thing;
  
  if (!job || !holder || !tra)
    return false;

  thing = holder->start;

  for (field = tra->first_field; field != NULL; field = field->next)
  {
    if (field->tag[0] != '@')
      continue;

    /*
      This is an array count. It should be set to the "elements"
      count of corresponding destination holder.
    */
    
    /* RFE: Check if it is a numeric type */

    SER_DPRINT("field %s %s - array length\n", field->type, field->tag);
    
    /* It's linked to the reference with the same name, without the @ */
    array_name = &field->tag[1];
    array_field = ser_find_field(tra, tra->id, array_name);
    
    if (!array_field)
    {
      SER_DPRINT("warning: couldn't determine array field\n");
      continue;
    }

    /* */
    sptr = *(ser_subst_ptr_t **)(thing + array_field->offset);
    
    array_target = sptr->d_hi;
    
    SER_DPRINT("%s %s seems to be holder %ld\n",
	       array_field->type, array_field->tag, array_target);
    
    if (array_target > job->allocated)
    {
      SER_DPRINT("warning: invalid holder index %ld\n", array_target);
      continue;
    }

    subst_holder = &job->holder[array_target];
    
    if (ser_is_signed(field->type))
      ser_write_signed(thing + field->offset, field->type, subst_holder->elements);
    else
      ser_write_unsigned(thing + field->offset, field->type, subst_holder->elements);
    
    SER_DPRINT("%s %s seems to be be %ld elements\n",
	       array_field->type, array_field->tag, subst_holder->elements);
  }

  return true;
}



/*
   #category Internal
*/
void ser_write_unsigned(void * ptr, const char * type, const unsigned long uvalue)
{
  if (strcmp(type, "ushort") == 0)
    *(unsigned short *)ptr = uvalue;
  else if (strcmp(type, "uint") == 0)
    *(unsigned int *)ptr = uvalue;
  else if (strcmp(type, "ulong") == 0)
    *(unsigned long *)ptr = uvalue;
  else if (strcmp(type, "size_t") == 0)
    *(size_t *)ptr = uvalue;
  else if (strcmp(type, "uint8_t") == 0)
    *(uint8_t *)ptr = uvalue;
  else if (strcmp(type, "uint16_t") == 0)
    *(uint16_t *)ptr = uvalue;
  else if (strcmp(type, "uint32_t") == 0)
    *(uint32_t *)ptr = uvalue;

  return;
}



/*
   #category Internal
*/
void ser_write_signed(void * ptr, const char * type, const signed long svalue)
{
  if (strcmp(type, "short") == 0)
    *(short *)ptr = svalue;
  else if (strcmp(type, "int") == 0)
    *(int *)ptr = svalue;
  else if (strcmp(type, "long") == 0)
    *(long *)ptr = svalue;
  else if (strcmp(type, "int8_t") == 0)
    *(int8_t *)ptr = svalue;
  else if (strcmp(type, "int16_t") == 0)
    *(int16_t *)ptr = svalue;
  else if (strcmp(type, "int32_t") == 0)
    *(int32_t *)ptr = svalue;
  else if (strcmp(type, "time_t") == 0)
    *(time_t *)ptr = svalue;

  return;
}



/**
   Checks if @type is a signed numeric type.

   #category Types
*/
bool ser_is_signed(const char * type)
{
  if (strcmp(type, "short") == 0 ||
      strcmp(type, "int") == 0 ||
      strcmp(type, "long") == 0 ||
      strcmp(type, "int8_t") == 0 ||
      strcmp(type, "int16_t") == 0 ||
      strcmp(type, "int32_t") == 0 ||
      strcmp(type, "time_t") == 0)
  {
    return true;
  }

  return false;
}



/**
   Returns a pointer to field @tag in the structure held by holder
   @holder_index in @job@; NULL if no such field could be found (it's
   missing in the @id translator) or any other error occurs.

   #category Internal
*/
void * ser_replace_ptr(ser_job_t * job,
		       const size_t holder_index,
		       const char * tag)
{
  char tmp[SER_MSGLEN];
  ser_tra_t * tra;
  ser_field_t * field;

  if (!job || !holder_index || !tag)
    return NULL;

  if (holder_index > job->allocated ||
      job->holder[holder_index].start == NULL)
  {
    snprintf(tmp, SER_MSGLEN,
	     "error: invalid redirect to holder %ld (it's NULL)", holder_index);
    job->log_func(tmp);
    return NULL;
  }

  tra = ser_find_tra(job->first_tra, job->holder[holder_index].type);
  
  if (!tra)
  {
    snprintf(tmp, SER_MSGLEN,
	     "error: lacking translator for holder %ld", holder_index);
    job->log_func(tmp);
    return NULL;
  }

  field = ser_find_field(tra, tra->id, tag);

  if (!field)
  {
    snprintf(tmp, SER_MSGLEN,
	     "error: redirecting to #%ld but %s doesn't have any field %s",
	     holder_index, tra->id, tag);
    job->log_func(tmp);
    return NULL;
  }

  return (job->holder[holder_index].start + field->offset);
}



/*
   #category Internal
*/
void ser_del_subst_ptrs(ser_job_t * job)
{
  ser_subst_ptr_t * sptr;
  ser_subst_ptr_t * next;

  if (!job)
    return;

  sptr = job->first_sptr;

  while (sptr)
  {
    next = sptr->next;

    if (sptr->d_tag)
      free(sptr->d_tag);

    free(sptr);

    sptr = next;
  }

  job->first_sptr = NULL;
  
  return;
}



/**
   Returns the size (in bytes) of @type@. If @ref is nonzero it will
   instead return the size of a pointer to such an array (which really
   will be sizeof(void*)).

   #category Types
*/
size_t ser_field_size(ser_job_t * job, const char * type, const int ref)
{
  if (!type)
    return 0;

  if (ref)
    return sizeof(void *);
  
  SER_IF_SIZEOF(int)
  else SER_IF_SIZEOF(short)
  else SER_IF_SIZEOF(char)
  else SER_IF_SIZEOF(long)
  else SER_IF_SIZEOF(float)
  else SER_IF_SIZEOF(double)
  else SER_IF_SIZEOF(long long)
  else SER_IF_SIZEOF(size_t)
  else SER_IF_SIZEOF(intptr_t)
  else SER_IF_SIZEOF(uint8_t)
  else SER_IF_SIZEOF(uint16_t)
  else SER_IF_SIZEOF(uint32_t)
  else SER_IF_SIZEOF(int8_t)
  else SER_IF_SIZEOF(int16_t)
  else SER_IF_SIZEOF(int32_t)
  else if (strcmp(type, "string") == 0)
    return sizeof(char *);
  else if (strcmp(type, "ushort") == 0)
    return sizeof(unsigned short);
  else if (strcmp(type, "uint") == 0)
    return sizeof(unsigned int);
  else if (strcmp(type, "ulong") == 0)
    return sizeof(unsigned long);
  
  if (job)
  {
    ser_tra_t * tra;
    tra = ser_find_tra(job->first_tra, type);
    
    if (tra)
      return tra->size;
  }

  return 0;
}



/**
   Reads a structure from @job@s input buffer and assigns it to holder
   @id@. The structure will be parsed according to @thing_tra@.

   Returns true on success, false on error.

   #category Internal
*/
bool ser_read_struct(ser_job_t * job, const long id, ser_tra_t * thing_tra)
{
  ser_holder_t * holder;

  if (!job || !thing_tra)
    return false;

  SER_DPRINT("read_struct: in %s #%ld\n", thing_tra->id, id);

  /* This is the holder we will load the struct into. */
  holder = &job->holder[id];

  /*
    Prepare memory to load the inflated struct. Find out how much the
    translator wants. If it is supposed to be a dynamic array, we will
    be able to reallocate it later.
  */
  holder->type = strdup(thing_tra->id);

  holder->start = ser_blank_struct(thing_tra);

  SER_DPRINT("read_struct: allocating %ld bytes for %s in holder %ld, ->start %p\n",
	     thing_tra->size, thing_tra->id, id, holder->start);

  if (holder->start == NULL)
  {
    job->log_func("error: out of memory");
    return false;
  }

  /*
    If this is a dynamic array type we pass the holder along. If it's
    a "flat" structure we won't allow reallocation.
  */
  if (thing_tra->atype == ser_flat)
    return ser_read_struct_internal(job, NULL, thing_tra, holder->start);
  else
    return ser_read_struct_internal(job, holder, thing_tra, holder->start);
}



/**
   Assumes @job@s input parsing is at the start of a structure (after
   the opening brace). Reads the structure into @ptr according to
   @thing_tra@. If @holder is non-NULL, it will be used to reallocate
   ptr (if needed; this is only used for dynamic arrays - do not pass
   it when reading static arrays or embedded structs).

   Returns true on success, false on error.

   #category Internal
*/
bool ser_read_struct_internal(ser_job_t * job, ser_holder_t * holder,
			      ser_tra_t * thing_tra, void * ptr)
{
  char line[SER_MSGLEN];
  char * tag;
  char * value;
  void * dest;
  ser_field_t * field;
  ser_tok_t r;
  long tmp;
  size_t repeat;
  size_t i;

  if (!job || !thing_tra || !ptr)
    return false;

  if (holder)
  {
    SER_DPRINT("ser_read_struct_internal: dynamic structure (holder != NULL)\n");
  }
  else
  {
    SER_DPRINT("ser_read_struct_internal: flat structure (holder == NULL)\n");
  }

  /*
    Read field identifiers + values for the structure. Do this until
    we receive a closing brace.
  */
  while (1)
  {
    if (thing_tra->atype == ser_flat)
    {
      SER_DPRINT("normal type: expecting field tag or closing brace...\n");

      r = ser_ntok(job, &tag, &tmp);

      if (r == ser_tok_close)
      {
	/*
	  RFE: This would be a good place to verify that all values
	  have been read for a structure, but we don't have such an option yet.
	*/

	SER_DPRINT("got closing brace; finishing structure\n");
	break;
      }
      else if (r != ser_tok_any)
      {
	/* These are error conditions. */
	if (r == ser_tok_end)
	{
	  snprintf(line, SER_MSGLEN,
		   "error: premature end of structure at byte %ld",
		   job->p_byte);
	  job->log_func(line);
	}
	else if (r != ser_tok_err)
	{
	  snprintf(line, SER_MSGLEN,
		   "error: unexpected token at byte %ld", job->p_byte);
	  job->log_func(line);
	}
	
	/*
	  ser_tok_err tokens should already have printed their own
	  message in ser_ntok, so we will skip them.
	*/

	return false;
      }

      /*
	Let's assume it was a tag we received. Find out if it exists
	in the current translator.
      */
      
      field = ser_find_field(thing_tra, thing_tra->id, tag);
      
      if (field == NULL)
      {
	snprintf(line, SER_MSGLEN,
		 "error: unknown field %s:%s at byte %ld",
		 thing_tra->id, tag, job->p_byte);
	job->log_func(line);
	return false;
      }
    }
    else if (thing_tra->atype == ser_array_dynamic ||
	     thing_tra->atype == ser_array_nullterm)
    {
      /*
	If the translator is a dynamic array we will only use the first
	field. We will _not_ expect a field tag in the input.
      */

      field = thing_tra->first_field;

      SER_DPRINT("%s is array type: using first field %s %s\n",
		 thing_tra->id, field->type, field->tag);
    }

    repeat = field->repeat;

    SER_DPRINT("field %s is %s%s (size %ld) at offset %ld, repeat %ld\n",
	       field->tag, field->type,
	       (field->ref ? " reference" : ""),
	       ser_field_size(job, field->type, field->ref),
	       field->offset, field->repeat);
    
    /* If we're expecting an array, we want an opening & closing brace */
    if (thing_tra->atype == ser_flat && field->repeat != 1)
    {
      if (ser_expect(job, ser_tok_open) == 0)
	return false;
    }

    /*
      If this is a dynamic array, we don't want any limit on how many
      elements we can read.
    */
    if (thing_tra->atype == ser_array_dynamic ||
	thing_tra->atype == ser_array_nullterm)
      repeat = UINTPTR_MAX;

    /* The field exists, we know what type to expect for value. */
    
    for (i = 0; i < repeat; i++)
    {
      dest =
	ptr + field->offset +
	ser_field_size(job, field->type, field->ref) * i;

      if (holder)
      {
	SER_DPRINT("holder %p, start %p, destination for value: %p\n",
		   holder, holder->start, dest);
      }

      /*      
	      if (strcmp(thing_tra->id, "creature_array") == 0)
	      {
	      SER_DPRINT("this is it\n");
	      }
      */

      r = ser_ntok(job, &value, &tmp);
	  
      if (r == ser_tok_err)
      {
	/* Errors should already have been reported in ser_ntok */
	return false;
      }
      else if (r == ser_tok_id)
      {
	if (!field->ref && strcmp(field->type, "string"))
	{
	  snprintf(line, SER_MSGLEN,
		   "error: got reference but field %s isn't a reference type at byte %ld",
		   field->tag, job->p_byte);
	  job->log_func(line);
	  return false;
	}

	/* Reference to other structure */
	SER_DPRINT("field %s is \"%s\" reference\n", field->tag, value);
	
	if (strcasecmp(value, "NULL") == 0)
	{
	  *(void **)dest = NULL;

	  SER_DPRINT("-- NULL\n");
	}
	else
	{
	  size_t d_hi;
	  char * d_tag;
	  size_t d_ai;
	  ser_subst_ptr_t * sptr;
	  
	  if (ser_dissect_reference(value, &d_hi, &d_tag, &d_ai) == 0)
	  {
	    snprintf(line, SER_MSGLEN,
		     "error: bad reference at byte %ld", job->p_byte);
	    job->log_func(line);
	    return false;
	  }
	  
	  sptr = ser_add_subst_ptr(job, d_hi, d_tag, d_ai);
	  
	  if (!sptr)
	  {
	    snprintf(line, SER_MSGLEN,
		     "error: couldn't create subst ptr for holder %ld", d_hi);
	    job->log_func(line);
	    return false;
	  }
	
	  *(void**)dest = sptr;

	  SER_DPRINT("-- subst ptr to %ld.%s[%ld]\n",
		     sptr->d_hi, sptr->d_tag, sptr->d_ai);
	}
      }
      else if (ser_read_value(job, field->type, r, dest, value, tmp) == 0)
      {
	return false;
      }

      if (holder && thing_tra->atype != ser_flat)
      {
	/* Update how many elements we have read */
	holder->elements++;
      }

      if (repeat > 1 && i == repeat - 1)
      {
	/* This is a fixed-size array. */
	SER_DPRINT("expecting closing brace...\n");

	if (ser_expect(job, ser_tok_close) == 0)
	  return false;

	/*
	  It's a static array. Check that we got the number of
	  elements we wanted. Set repeat to 1 so we won't expect
	  another closing brace.
	*/
	
	if (i < repeat - 1)
	{
	  snprintf(line, SER_MSGLEN,
		   "warning: too few values received for %s:%s",
		   thing_tra->id, field->tag);
	  job->log_func(line);
	}
      }
      else if (i < repeat - 1)
      {
	/* This only happens for repeat > 0 */

	SER_DPRINT("expecting separator or closing brace...\n");

	r = ser_ntok(job, &value, &tmp);

	if (r == ser_tok_err)
	  return false;
	else if (r == ser_tok_close)
	{
	  /*
	    Dynamic arrays don't need any closing brace for the
	    __internal__ field; one is enough to close the whole
	    structure.
	  */

	  if (thing_tra->atype == ser_array_dynamic ||
	      thing_tra->atype == ser_array_nullterm)
	  {
	    return true;
	  }

	  break;
	}
	else if (r != ser_tok_sep)
	{
	  snprintf(line, SER_MSGLEN,
		   "error: expecting separator, got %s at byte %ld",
		   ser_token_code(r), job->p_byte);
	  job->log_func(line);
	  return false;
	}
	else
	{
	  /* Got a separator */

	  if (holder)
	  {
	    size_t nsize;
	    nsize = thing_tra->size * (holder->elements + 1);

	    SER_DPRINT("reallocating %ld elements\n", holder->elements + 1);
	    SER_DPRINT("field size for %s: %ld\n", thing_tra->id, thing_tra->size);
	    SER_DPRINT("total %ld bytes\n", nsize);
	    
	    ptr = realloc(holder->start, nsize);

	    if (!ptr)
	    {
	      free(holder->start);
	      job->log_func("out of memory");
	      return false;
	    }

	    holder->start = ptr;
	  }
	  else if (repeat == 1)
	  {
	    snprintf(line, SER_MSGLEN,
		     "error: got %s at byte %ld, but not expecting array",
		     ser_token_code(r), job->p_byte);
	    job->log_func(line);
	    return false;
	  }
	}
      }
    } /* for repeat */

    /*
      For flat structures we want terminators after each value.
    */
    if (thing_tra->atype == ser_flat)
    {
      if (ser_expect(job, ser_tok_term) == 0)
	return false;
    }
  } /* while r */

  return true;
}



/**
   Reads a token from @job@s input buffer. Returns true if it is of
   type @expected@, otherwise false. If it is of another type a
   message will be sent to @job@s log_func. The output will be
   discarded (this is mostly useful for detecting braces and
   terminators).

   #category Internal
*/
bool ser_expect(ser_job_t * job, ser_tok_t expected)
{
  char line[SER_MSGLEN];
  ser_tok_t r;
  char * value;
  long tmp;

  if (!job)
    return false;

  SER_DPRINT("expecting %s\n", ser_token_code(expected));
  
  r = ser_ntok(job, &value, &tmp);
  
  if (r == ser_tok_err)
  {
    /* Error message already printed in ser_ntok */
    return false;
  }
  else if (r != expected)
  {
    snprintf(line, SER_MSGLEN,
	     "error: expecting %s, got %s at byte %ld",
	     ser_token_code(expected),
	     ser_token_code(r),
	     job->p_byte);
    job->log_func(line);
    return false;
  }

  return true;
}



/**
   Reads @token into @where. @r is the type of token provided and must
   match the @field. If the field is a numeric @value will be used
   instead of @token.

   Returns true on success, false on failure.

   #category Internal
*/
bool ser_read_value(ser_job_t * job, const char * type, ser_tok_t r,
		    void * where, char * token, long int value)
{
  char line[SER_MSGLEN];

  if (!job || /*!field ||*/ !where || !token)
    return false;

  SER_DPRINT("ser_read_value()\n");

  if (strcmp(type, "string") == 0)
  {
    if (r == ser_tok_any && strcmp(token, "NULL") == 0)
    {
      SER_DPRINT("field is NULL - OK!\n");
      *(char **)where = NULL;
    }
    else
    {
      if (r != ser_tok_string)
      {
	snprintf(line, SER_MSGLEN,
		 "error: expecting %s, got %s at byte %ld",
		 type, ser_token_code(r), job->p_byte);
	job->log_func(line);
	return false;
      }
      
      char * dup_value;
      
      dup_value = ser_unescape_str(token);
      
      if (!dup_value)
      {
	job->log_func("error: out of memory");
	return false;
      }
      
      /* Success! */
      *(char **)where = dup_value;
      
      SER_DPRINT("field is \"%s\" - OK!\n", dup_value);
    }
  }
  else if (strcmp(type, "float") == 0 ||
	   strcmp(type, "double") == 0 ||
	   strcmp(type, "ldouble") == 0)
  {
    long double num;
    char * endptr;

    num = strtold(token, &endptr);

    if (*endptr != '\0')
    {
      snprintf(line, SER_MSGLEN,
	       "error: expecting %s, got %s at byte %ld",
	       type, ser_token_code(r), job->p_byte);
      job->log_func(line);
      return false;
    }

    if (strcmp(type, "float") == 0)
      *(float*)where = num;
    else if (strcmp(type, "double") == 0)
      *(double*)where = num;
    else if (strcmp(type, "ldouble") == 0)
      *(long double*)where = num;
  }
  else if (strcmp(type, "bool") == 0)
  {
    if (value)
      *(bool*)where = true;
    else
      *(bool*)where = false;
  }
  else if (strcmp(type, "ushort") == 0 ||
	   strcmp(type, "short") == 0 ||
	   strcmp(type, "uint") == 0 ||
	   strcmp(type, "int") == 0 ||
	   strcmp(type, "ulong") == 0 ||
	   strcmp(type, "long") == 0 ||
	   strcmp(type, "time_t") == 0 ||
	   strcmp(type, "size_t") == 0 ||
	   strcmp(type, "uint8_t") == 0 ||
	   strcmp(type, "uint16_t") == 0 ||
	   strcmp(type, "uint32_t") == 0 ||
	   strcmp(type, "int8_t") == 0 ||
	   strcmp(type, "int16_t") == 0 ||
	   strcmp(type, "int32_t") == 0)
  {
    signed long svalue;
    unsigned long uvalue;
    int sign;
    char * endptr;

    if (ser_is_signed(type))
    {
      svalue = strtol(token, &endptr, 10);
      sign = 1;
    }
    else
    {
      uvalue = strtoul(token, &endptr, 10);
      sign = 0;
    }
    
    if (*endptr != '\0')
    {
      snprintf(line, SER_MSGLEN,
	       "error: expecting %s, got %s at byte %ld",
	       type, ser_token_code(r), job->p_byte);
      job->log_func(line);
      return false;
    }

    if (sign)
      ser_write_signed(where, type, svalue);
    else
      ser_write_unsigned(where, type, uvalue);

    SER_DPRINT("field is %ld - OK!\n", value);
  }
  else if (strcmp(type, "char") == 0)
  {
    if (r != ser_tok_char)
    {
      sprintf(line, "error: expecting %s, got %s at byte %ld",
	      type, ser_token_code(r), job->p_byte);
      job->log_func(line);
      return false;
    }
    
    SER_DPRINT("field is %c - OK!\n", (char)value);

    *(char *)where = (char)value;
  }
  else if (r == ser_tok_open)
  {
    SER_DPRINT("read_value: got opening brace; anonymous struct\n");

    ser_tra_t * anon_tra;
    anon_tra = ser_find_tra(job->first_tra, type);

    if (anon_tra)
    {
      if (ser_read_struct_internal(job, NULL, anon_tra, where) == 0)
	return false;
    }
    else
    {
      snprintf(line, SER_MSGLEN,
	       "error: no translator for anonymous %s at byte %ld",
	       type, job->p_byte);
      job->log_func(line);
      return false;
    }
  }
  else if (r != ser_tok_any && r != ser_tok_id)
  {
    snprintf(line, SER_MSGLEN,
	     "error: expecting field value, got %s at byte %ld",
	     ser_token_code(r), job->p_byte);
    job->log_func(line);
    return false;
  }
  
  return true;
}



/**
   Extracts a new token from @job@s parse buffer. Returns the type of
   token, ser_tok_any if it can't determine what it is. @dest will be
   repointed to the string representation of the token. @value will be
   set to the numerical value, if it can be determined.
   
   On error, ser_tok_err is returned and a message will be printed
   using the log_func of @job. In these cases, it is usually not
   necessary to print any other error messages.

   #category Internal
*/
ser_tok_t ser_ntok(ser_job_t * job, char ** dest, long int * value)
{
  char line[SER_MSGLEN];

  ptrdiff_t comment_start;

  char * start;
  char * end;

  if (!job || !dest || !value)
    return ser_tok_err;

  start = job->p_pos;

strip_whitespace:
  while (*start == ' ' || *start == '\n' || *start == '\t')
  {
    start++;
  }

  job->p_byte = start - job->p_str;

  if (*start == '\0')
    return ser_tok_end;
  
  /* Bypass comments */
  if (*start == '/' && *(start+1) == '*')
  {
    comment_start = start - job->p_str;

    start += 2;

    while (1)
    {
      if (*start == '\0')
      {
	snprintf(line, SER_MSGLEN,
		 "error: unterminated comment starting at byte %ld",
		 comment_start);
	job->log_func(line);
	return ser_tok_err;
      }
      else if (*start == '*' && *(start+1) == '/')
      {
	start += 2;
	goto strip_whitespace;
      }
      else
      {  
	start++;
      }
    }
  }

  if (*start == '\0')
    return ser_tok_end;
  
  end = start;
    
  if (*start == ';' || *start == ',')
  {
  }
  else if (*start == '\"')
  {
    do
    {
      if (*end == '\\')
      {
	end++;

	if (*end == '\\' || *end == '\"')
	  end++;
      }
      else
	end++;
    } while (*end != '\"' && *end != '\0');

    if (*end == '\0')
    {
      snprintf(line, SER_MSGLEN,
	       "error: premature end of string at byte %ld",
	       job->p_byte);
      job->log_func(line);
      return ser_tok_err;
    }

    *end = '\0';
  }
  else if (*start == '\'')
  {
    end++;

    while (*end != '\'')
    {
      if (*end == '\0')
      {
	snprintf(line, SER_MSGLEN,
		 "error: broken character at byte %ld",
		 job->p_byte);
	job->log_func(line);
	return ser_tok_err;
      }

      end++;
    }

    /* Found the ' */
    end++;
    *end = '\0';
  }
  else
  {
    while (*end != ' ' && *end != '\n' &&
	   *end != '\t' && *end != '\0' &&
	   *end != ';')
    {
      end++;
    }

    *end = '\0';
  }

  job->p_pos = end + 1;
  
  *dest = start;

  job->p_byte = start - job->p_str;

  if (*start == ';' || *start == ',')
  {
    SER_DPRINT("@ %ld >%c<\n", start - job->p_str, *start);

    if (*start == ';')
      return ser_tok_term;
    else if (*start == ',')
      return ser_tok_sep;
  }

  SER_DPRINT("@ %ld >%s<\n", start - job->p_str, start);

  if (*start == '\"')
  {
    /*
      This is a string token. The endpoint should already have reached
      the terminating quote and replaced it with a null byte, so we
      only need to move the start past the opening quote.
    */

    start++;
    (*dest)++;

    return ser_tok_string;
  }
  else if (*start == '\'')
  {
    SER_DPRINT("char %s, %c %c %c\n",
	       start, *(start + 1), *(start + 2), *(start + 3));

    /* It's a character. Is it backslash-escaped hex? */

    if (*(start + 1) == '\\' && *(start + 2) == 'x')
    {
      *value = ser_hex2char(start + 3);

      SER_DPRINT("escaped %ld\n", *value);

      return ser_tok_char;
    }

    *value = *(start + 1);
    return ser_tok_char;
  }
  else if (*start == '#')
  {
    /*
      It's a reference to another serialized structure.
      Try to extract the integer value.
    */

    char * endptr;

    *value = strtol(start + 1, &endptr, 10);

    if (endptr == start + 1)
    {
      snprintf(line, SER_MSGLEN,
	       "error: bad reference at byte %ld: %s", end - job->p_str, start);
      job->log_func(line);
      return ser_tok_err;
    }

    return ser_tok_id;
  }
  else if (*start == '{')
    return ser_tok_open;
  else if (*start == '}')
    return ser_tok_close;
  else if (strcasecmp(start, "true") == 0)
  {
    *value = true;
    return ser_tok_int;
  }
  else if (strcasecmp(start, "false") == 0)
  {
    *value = false;
    return ser_tok_int;
  }
  else if (strcasecmp(start, "NULL") == 0)
  {
    *value = 0;
    return ser_tok_id;
  }

  char * endptr;
  
  *value = strtol(start, &endptr, 10);
  
  if (endptr == end)
    return ser_tok_int;

  return ser_tok_any;
}



/**
   Returns if @s is a type that can be serialized natively (without a
   user-provided translator).

   #category Types
*/
bool ser_is_primitive(const char * s)
{
  if (!s)
    return false;

  if (strcmp(s, "string") == 0 ||
      strcmp(s, "bool") == 0 ||
      strcmp(s, "char") == 0 ||
      strcmp(s, "short") == 0 ||
      strcmp(s, "ushort") == 0 ||
      strcmp(s, "int") == 0 ||
      strcmp(s, "uint") == 0 ||
      strcmp(s, "long") == 0 ||
      strcmp(s, "ulong") == 0 ||
      strcmp(s, "size_t") == 0 ||
      strcmp(s, "time_t") == 0 ||
      strcmp(s, "intptr_t") == 0 ||
      strcmp(s, "uint8_t") == 0 ||
      strcmp(s, "uint16_t") == 0 ||
      strcmp(s, "uint32_t") == 0 ||
      strcmp(s, "int8_t") == 0 ||
      strcmp(s, "int16_t") == 0 ||
      strcmp(s, "int32_t") == 0 ||
      strcmp(s, "float") == 0 ||
      strcmp(s, "double") == 0 ||
      strcmp(s, "ldouble") == 0)
  {
    return true;
  }

  return false;
}



/**
   Given the linked list of translators @tra, finds the one called @id@.

   #category Translators
*/
ser_tra_t * ser_find_tra(ser_tra_t * tra, const char * id)
{
  while (tra)
  {
    if (strcmp(tra->id, id) == 0)
      return tra;
    
    tra = tra->next;
  }

  return NULL;
}



/**
   Returns if @s is a valid type (primitive or present as a translator
   in translator list @tra@).

   #category Types
*/
bool ser_valid_type(ser_tra_t * tra, char * s)
{
  if (!s)
    return false;

  if (ser_is_primitive(s))
    return true;

  if (!tra)
    return false;

  if (ser_find_tra(tra, s))
    return true;

  return false;
}



/**
   Given the translator list @tra@, looks up field @tag in translator @id@.

   Returns a pointer to the field or NULL if it could not be
   found. Both translator @id and field @tag must match; if there is
   another translator with the same field tag it will not be detected.

   #category Fields
*/
ser_field_t * ser_find_field(ser_tra_t * tra, const char * id, const  char * tag)
{
  ser_field_t * f;

  if (!tra || !id || !tag)
    return NULL;

  while (tra)
  {
    if (strcmp(tra->id, id) == 0)
    {
      for (f = tra->first_field; f != NULL; f = f->next)
      {
	if (strcmp(f->tag, tag) == 0)
	{
	  return f;
	}
      }
    }
    
    tra = tra->next;
  }

  return NULL;
}




/**
   Returns the field in @tra with offset @offset@, or NULL if none is found.

   #category Internal
*/
ser_field_t * ser_field_by_offset(ser_tra_t * tra, const size_t offset)
{
  ser_field_t * f;

  if (!tra)
    return NULL;

  for (f = tra->first_field; f != NULL; f = f->next)
  {
    if (f->offset == offset)
    {
      return f;
    }
  }
    
  return NULL;
}




/**
   Allocates and blanks memory for a structure of type @tra@.

   #category Internal
*/
void * ser_blank_struct(ser_tra_t * tra)
{
  void * ret;

  if (!tra)
    return NULL;

  ret = calloc(1, tra->size);

  return ret;
}



/**
   Creates a copy of @s with quotes, backslashes and non-printable
   characters preceded by backslashes. Newline characters (ASCII 10)
   will be represented as \n, quotes as \", backslashes as \\ and
   non-printable characters as \xnn, where nn are hexadecimal digits.

   Returns the escaped string or NULL on error.

   #category Strings
*/
char * ser_escape_str(char * s)
{
  char * a;
  char * b;
  char * ret;
  int c;

  if (!s)
    return NULL;

  c = 0;
  a = s;

  while (*a)
  {
    if (*a == '\n')
      c++;
    else if (!isprint(*a))
      c += 3;
    else if (*a == '\"' || *a == '\\')
      c++;

    c++;
    a++;
  }

  c++; /* Terminator */

  ret = malloc(c);

  if (!ret)
    return NULL;

  a = s;
  b = ret;

  while (*a)
  {
    if (*a == '\n')
    {
      *b++ = '\\';
      *b++ = 'n';
      a++;
      continue;
    }
    else if (!isprint(*a))
    {
      *b++ = '\\';
      *b++ = 'x';
      sprintf(b, "%02X", *a % 256);
      b += 2;
      a++;
      continue;
    }
    else if (*a == '\"' || *a == '\\')
      *b++ = '\\';

    *b++ = *a++;
  }

  *b = '\0';

  return ret;
}


/**
   Creates a copy of @s with escaped characters converted back to
   their real values; reverse of ser_escape_str.

   Returns the restored string or NULL on error.

   #category Strings
*/
char * ser_unescape_str(char * s)
{
  char * a;
  char * b;
  char * ret;

  if (!s)
    return NULL;

  ret = malloc(strlen(s) + 1);

  if (!ret)
    return NULL;

  a = s;
  b = ret;

  while (*a)
  {
    if (*a == '\\')
    {
      a++;

      if (*a == 'n')
      {
	*b++ = '\n';
	a++;
	continue;
      }
      else if (*a == 'x')
      {
	a++;
	*b++ = ser_hex2char(a);

	if (*a++ == '\0' || *a++ == '\0')
	  break;

	continue;
      }
      else if (*a != '\"' && *a != '\\')
	*b++ = '\\';
    }

    *b++ = *a++;
  }

  *b = '\0';

  /* Free space we don't need */
  ret = realloc(ret, strlen(ret) + 1);
  
  return ret;
}



/**
   Converts two hexadecimal digits to a number. @s should point to a
   string with two characters 0-F (case insensitive); these will be
   converted to a value in the 0-255 range.

   Returns the converted value.

   #category Strings
*/
char ser_hex2char(char * s)
{
  int i;
  int ret;

  if (!s)
    return 0;

  ret = 0;

  i = 2;
  
  while (i--)
  {
    ret <<= 4;

    if (*s == '\0')
      break;

    switch (*s)
    {
      case '0': break;
      case '1': ret += 1; break;
      case '2': ret += 2; break;
      case '3': ret += 3; break;
      case '4': ret += 4; break;
      case '5': ret += 5; break;
      case '6': ret += 6; break;
      case '7': ret += 7; break;
      case '8': ret += 8; break;
      case '9': ret += 9; break;
      case 'a': case 'A': ret += 10; break;
      case 'b': case 'B': ret += 11; break;
      case 'c': case 'C': ret += 12; break;
      case 'd': case 'D': ret += 13; break;
      case 'e': case 'E': ret += 14; break;
      case 'f': case 'F': ret += 15; break;
      default: break;
    }

    s++;
  }
  
  return ret;
}



char * ser_token_codes[] =
{
  [ser_tok_err] = "error",
  [ser_tok_end] = "end",
  [ser_tok_id] = "id",
  [ser_tok_open] = "opening brace",
  [ser_tok_close] = "closing brace",
  [ser_tok_term] = "terminator",
  [ser_tok_string] = "string",
  [ser_tok_any] = "any",
  [ser_tok_int] = "int",
  [ser_tok_char] = "char",
  [ser_tok_sep] = "separator"
};



/**
   Returns a pointer to a string describing @token.

   #category Internal
*/
char * ser_token_code(const ser_tok_t token)
{
  if (token < 0 || token > ser_tok_int)
    return ser_token_codes[0];
  else
    return ser_token_codes[token];
}
