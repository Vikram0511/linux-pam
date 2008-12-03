/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "pam_private.h"

#include <security/pam_ext.h>

#define PROMPT _("Password: ")
/* For Translators: "%s%s" could be replaced with "<service> " or "". */
#define PROMPT1 _("New %s%spassword: ")
/* For Translators: "%s%s" could be replaced with "<service> " or "". */
#define PROMPT2 _("Retype new %s%spassword: ")
#define MISTYPED_PASS _("Sorry, passwords do not match.")

static const char *
get_option (pam_handle_t *pamh, const char *option)
{
  int i;
  size_t len;


  if (option == NULL || pamh == NULL ||
      pamh->mod_argc == 0 || pamh->mod_argv == NULL)
    return NULL;

  len = strlen (option);

  for (i = 0; i < pamh->mod_argc; i++)
    {
      if (strncmp (option, pamh->mod_argv[i], len) == 0)
        {
          if (pamh->mod_argv[i][len] == '=')
            return &(pamh->mod_argv[i][len+1]);
          else if (pamh->mod_argv[i][len] == '\0')
            return "";
        }
    }
  return NULL;
}


int
pam_get_authtok (pam_handle_t *pamh, int item, const char **authtok,
		 const char *prompt)

{
  char *resp[2] = {NULL, NULL};
  const void* prevauthtok;
  const char *type = "";
  int ask_twice = 0; /* Password change, ask twice for it */
  int retval;

  if (authtok == NULL)
    return PAM_SYSTEM_ERR;

  /* PAM_AUTHTOK in password stack returns new password,
     which needs to be verified. */
  if (item == PAM_AUTHTOK && pamh->choice == PAM_CHAUTHTOK)
    {
      ask_twice = 1;
      type = get_option (pamh, "type");
      if (type == NULL)
	type = "";
    }

  retval = pam_get_item (pamh, item, &prevauthtok);
  if (retval == PAM_SUCCESS && prevauthtok != NULL)
    {
      *authtok = prevauthtok;
      return PAM_SUCCESS;
    }
  else if (get_option (pamh, "use_first_pass") ||
	   (ask_twice && get_option (pamh, "use_authtok")))
    {
      if (prevauthtok == NULL)
	{
	  if (ask_twice)
	    return PAM_AUTHTOK_ERR;
	  else
	    return PAM_AUTH_ERR;
	}
      else
	return retval;
    }

  if (prompt != NULL)
    {
      retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
			   "%s", prompt);
      if (retval == PAM_SUCCESS && ask_twice && resp[0] != NULL)
	retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[1],
			     _("Retype %s"), prompt);
    }
  else if (ask_twice)
    {
      retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0],
			   PROMPT1, type,
			   strlen (type) > 0?" ":"");
      if (retval == PAM_SUCCESS && ask_twice && resp[0] != NULL)
	retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[1],
			     PROMPT2, type,
			     strlen (type) > 0?" ":"");
    }
  else
    retval = pam_prompt (pamh, PAM_PROMPT_ECHO_OFF, &resp[0], "%s",
			 PROMPT);

  if (resp[0] == NULL || (ask_twice && resp[1] == NULL))
    {
      /* We want to abort the password change */
      pam_error (pamh, _("Password change aborted."));
      return PAM_AUTHTOK_ERR;
    }

  if (ask_twice && strcmp (resp[0], resp[1]) != 0)
    {
      pam_error (pamh, MISTYPED_PASS);
      _pam_overwrite (resp[0]);
      _pam_drop (resp[0]);
      _pam_overwrite (resp[1]);
      _pam_drop (resp[1]);
      return PAM_TRY_AGAIN;
    }

  _pam_overwrite (resp[1]);
  _pam_drop (resp[1]);

  retval = pam_set_item (pamh, item, resp[0]);
  _pam_overwrite (resp[0]);
  _pam_drop (resp[0]);
  if (retval != PAM_SUCCESS)
    return retval;

  return pam_get_item(pamh, item, (const void **)authtok);
}