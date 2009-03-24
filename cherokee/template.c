/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2009 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */ 

#include "common-internal.h"
#include "template.h"

/* Template Replacements
 */

typedef struct {
	cherokee_list_t            listed;
	cuint_t                    pos;
	cherokee_template_token_t *token;
} cherokee_template_replacement_t;

#define TEMPLATE_REPL(x) ((cherokee_template_replacement_t *)(x))

static ret_t
replacement_new (cherokee_template_replacement_t **repl)
{
	CHEROKEE_NEW_STRUCT (n, template_replacement);

	n->pos   = 0;
	n->token = NULL;
	INIT_LIST_HEAD (&n->listed);

	*repl = n;
	return ret_ok;
}

static ret_t
replacement_free (cherokee_template_replacement_t *repl)
{
	free (repl);
	return ret_ok;
}


/* Template Tokens
 */

static ret_t
token_new (cherokee_template_token_t **token)
{
	CHEROKEE_NEW_STRUCT (n, template_token);

	INIT_LIST_HEAD (&n->listed);
	cherokee_buffer_init (&n->key);

	n->func  = NULL;
	n->param = NULL;

	*token = n;
	return ret_ok;
}

static ret_t
token_free (cherokee_template_token_t *token)
{
	cherokee_buffer_mrproper (&token->key);

	free (token);
	return ret_ok;
}


/* Template
 */

ret_t
cherokee_template_init (cherokee_template_t *tem)
{
	cherokee_buffer_init (&tem->text);
	INIT_LIST_HEAD (&tem->tokens);
	INIT_LIST_HEAD (&tem->replacements);

	return ret_ok;
}


ret_t
cherokee_template_mrproper (cherokee_template_t *tem)
{
	cherokee_list_t *i, *tmp;

	list_for_each_safe (i, tmp, &tem->tokens) { 
		token_free (TEMPLATE_TOKEN(i));
	}

	list_for_each_safe (i, tmp, &tem->replacements) { 
		replacement_free (TEMPLATE_REPL(i));
	}

	cherokee_buffer_mrproper (&tem->text);
	return ret_ok;
}


ret_t
cherokee_template_new_token (cherokee_template_t        *tem,
			     cherokee_template_token_t **token)
{
	ret_t ret;

	ret = token_new (token);
	if (unlikely (ret != ret_ok))
		return ret;

	cherokee_list_add (&(*token)->listed, &tem->tokens);
	return ret_ok;
}


ret_t
cherokee_template_set_token  (cherokee_template_t        *tem,
			      const char                 *name,
			      cherokee_tem_repl_func_t    func,
			      void                       *param,
			      cherokee_template_token_t **token)
{	
	ret_t                      ret;
	cherokee_template_token_t *n;

	ret = cherokee_template_new_token (tem, &n);
	if (unlikely (ret != ret_ok))
		return ret;

	cherokee_buffer_add (&n->key, name, strlen(name));
	n->func  = func;
	n->param = param;
	
	if (token) {
		*token = n;
	}

	return ret_ok;
}


ret_t
cherokee_template_parse (cherokee_template_t *tem,
			 cherokee_buffer_t   *incoming)
{
	ret_t              ret;
	char              *token1;
	char              *token2;
	cherokee_list_t   *i;
	char              *p     = incoming->buf;
	char              *end   = incoming->buf + incoming->len;
	cherokee_buffer_t  token = CHEROKEE_BUF_INIT;

	while (p < end) {
		cherokee_template_replacement_t *repl;

		/* Find the next token
		 */
		token1 = strstr (p, "${");
		if (token1 == NULL) {
			cherokee_buffer_add (&tem->text, p, end-p);
			ret = ret_ok;
			goto out;
		}
		
		token2 = strchr (p+2, '}');
		if (token2 == NULL) {
			cherokee_buffer_add (&tem->text, p, end-p);
			ret = ret_ok;
			goto out;
		}

		/* Copy the text before the token
		 */
		cherokee_buffer_add (&tem->text, p, token1 - p);

		cherokee_buffer_clean (&token);
		cherokee_buffer_add   (&token, token1+2, (token2-token1)-2);

		/* Log the token and skip it
		 */
		ret = replacement_new (&repl);
		if (ret != ret_ok) {
			goto out;
		}

		repl->pos = tem->text.len;
		cherokee_list_add_tail (&repl->listed, &tem->replacements);

		list_for_each (i, &tem->tokens) {
			cherokee_template_token_t *token_i = TEMPLATE_TOKEN(i);

			if (cherokee_buffer_cmp_buf (&token, &token_i->key) == 0) {
				repl->token = token_i;
				break;
			}
		}
		
		if (unlikely (repl->token == NULL)) {
			PRINT_ERROR ("Template: Token not found '%s'\n", token.buf);
			ret = ret_error;
			goto out;
		}

		/* Get ready for the next one
		 */
		p = token2+1;
	}

out:
	cherokee_buffer_mrproper (&token);
	return ret;
}

ret_t
cherokee_template_parse_file (cherokee_template_t *tem,
			      const char          *file)
{
	ret_t             ret;
	cherokee_buffer_t tmp = CHEROKEE_BUF_INIT;

	ret = cherokee_buffer_read_file (&tmp, (char *)file);
	if (ret != ret_ok)
		return ret;

	ret = cherokee_template_parse (tem, &tmp);
	if (ret != ret_ok) {
		ret = ret_error;
		goto out;
	}
		
	ret = ret_ok;

out:
	cherokee_buffer_mrproper (&tmp);
	return ret;
}


ret_t
cherokee_template_render (cherokee_template_t *tem,
			  cherokee_buffer_t   *output,
			  void                *param)
{
	ret_t                            ret;
	cherokee_list_t                 *i;
	cherokee_template_replacement_t *repl;	
	cuint_t                          pos   = 0;

	list_for_each (i, &tem->replacements) {
		repl = TEMPLATE_REPL(i);
		
		/* Copy the string preceding the token
		 */
		if (repl->pos > 0) {
			cherokee_buffer_add (output,
					     tem->text.buf + pos,
					     repl->pos - pos);
		}

		/* Add the token
		 */
		ret = repl->token->func (tem, repl->token, output, param);
		if (unlikely (ret != ret_ok))
			return ret;

		pos = repl->pos;
	}

	/* Copy the last chunk
	 */
	cherokee_buffer_add (output,
			     tem->text.buf + pos,
			     tem->text.len - pos);

	return ret_ok;
}
