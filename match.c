/*
 * Copyright (c) 1996, 1998-2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#endif /* HAVE_FNMATCH */
#ifdef HAVE_EXTENDED_GLOB
# include <glob.h>
#endif /* HAVE_EXTENDED_GLOB */
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "sudo.h"
#include "interfaces.h"
#include "parse.h"
#include "gram.h"

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */
#ifndef HAVE_EXTENDED_GLOB
# include "emul/glob.h"
#endif /* HAVE_EXTENDED_GLOB */

#ifndef lint
static const char rcsid[] = "$Sudo$";
#endif /* lint */

/*
 * Prototypes
 */
static int has_meta	__P((char *));

/*
 * Check for user described by pw in a list of members.
 * Returns ALLOW, DENY or UNSPEC.
 */
int
user_matches(pw, list)
    struct passwd *pw;
    struct member *list;
{
    struct member *m;
    int rval, matched = UNSPEC;

    for (m = list; m != NULL; m = m->next) {
	switch (m->type) {
	    case ALIAS:
		rval = alias_matches(m->name, USERALIAS, pw, NULL);
		if (rval != UNSPEC || (rval = !strcmp(m->name, pw->pw_name)))
		    matched = rval;
		break;
	    case ALL:
		matched = !m->negated;
		break;
	    case NETGROUP:
		if (netgr_matches(m->name, NULL, NULL, pw->pw_name))
		    matched = !m->negated;
		break;
	    case USERGROUP:
		if (usergr_matches(m->name, pw->pw_name, pw))
		    matched = !m->negated;
		break;
	    case WORD:
		if (strcmp(m->name, pw->pw_name) == 0)
		    matched = !m->negated;
		break;
	}
    }
    return(matched);
}

/*
 * Check for user described by pw in a list of members.
 * If list is NULL compare against def_runas_default.
 * Returns ALLOW, DENY or UNSPEC.
 */
int
runas_matches(pw, list)
    struct passwd *pw;
    struct member *list;
{
    if (list == NULL)
	return(strcmp(pw->pw_name, def_runas_default) == 0);
    return(user_matches(pw, list));
}

/*
 * Check for host and shost in a list of members.
 * Returns ALLOW, DENY or UNSPEC.
 */
int
host_matches(shost, lhost, list)
    char *shost, *lhost;
    struct member *list;
{
    struct member *m;
    int rval, matched = UNSPEC;

    for (m = list; m != NULL; m = m->next) {
	switch (m->type) {
	    case ALIAS:
		rval = alias_matches(m->name, HOSTALIAS, shost, lhost);
		if (rval != UNSPEC || hostname_matches(shost, lhost, m->name))
		    matched = rval;
		break;
	    case ALL:
		matched = !m->negated;
		break;
	    case NETGROUP:
		if (netgr_matches(m->name, lhost, shost, NULL))
		    matched = !m->negated;
		break;
	    case NTWKADDR:
		if (addr_matches(m->name))
		    matched = !m->negated;
		break;
	    case WORD:
		if (hostname_matches(shost, lhost, m->name))
		    matched = !m->negated;
		break;
	}
    }
    return(matched);
}

/*
 * Check for cmnd and args in a list of members.
 * Returns ALLOW, DENY or UNSPEC.
 * XXX - never called in list context
 */
int
cmnd_matches(cmnd, args, list)
    char *cmnd, *args;
    struct member *list;
{
    struct member *m;
    struct sudo_command *c;
    int rval, matched = UNSPEC;

    for (m = list; m != NULL; m = m->next) {
	switch (m->type) {
	    case ALIAS:
		rval = alias_matches(m->name, CMNDALIAS, cmnd, args);
		if (rval != UNSPEC)
		    matched = rval;
		break;
	    case ALL:
		matched = !m->negated;
		break;
	    case COMMAND:
		c = (struct sudo_command *)m->name;
		if (command_matches(c->cmnd, c->args))
		    matched = !m->negated;
		break;
	}
    }
    return(matched);
}

/*
 * Looks through the alias list for one with a matching name and type
 * and calls {host,cmnd,user}_matches() with the aliases.
 * Returns ALLOW, DENY or UNSPEC.
 */
int
alias_matches(name, type, v1, v2)
    char *name;
    int type;
    VOID *v1;
    VOID *v2;
{
    struct alias *a;

    if ((a = find_alias(name, type)) != NULL) {
	switch (type) {
	    case HOSTALIAS:
		return(host_matches(v1, v2, a->first_member));
	    case CMNDALIAS:
		return(cmnd_matches(v1, v2, a->first_member));
	    case USERALIAS:
	    case RUNASALIAS:
		return(user_matches(v1, a->first_member));
	}
    }
    return(UNSPEC);
}

/*
 * If path doesn't end in /, return TRUE iff cmnd & path name the same inode;
 * otherwise, return TRUE if user_cmnd names one of the inodes in path.
 */
int
command_matches(sudoers_cmnd, sudoers_args)
    char *sudoers_cmnd;
    char *sudoers_args;
{
    struct stat sudoers_stat;
    struct dirent *dent;
    char **ap, *base, buf[PATH_MAX];
    glob_t gl;
    DIR *dirp;

    /* Check for pseudo-commands */
    if (strchr(user_cmnd, '/') == NULL) {
	/*
	 * Return true if both sudoers_cmnd and user_cmnd are "sudoedit" AND
	 *  a) there are no args in sudoers OR
	 *  b) there are no args on command line and none req by sudoers OR
	 *  c) there are args in sudoers and on command line and they match
	 */
	if (strcmp(sudoers_cmnd, "sudoedit") != 0 ||
	    strcmp(user_cmnd, "sudoedit") != 0)
	    return(FALSE);
	if (!sudoers_args ||
	    (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
	    (sudoers_args &&
	     fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
	    if (safe_cmnd)
		free(safe_cmnd);
	    safe_cmnd = estrdup(sudoers_cmnd);
	    return(TRUE);
	} else
	    return(FALSE);
    }

    /*
     * If sudoers_cmnd has meta characters in it, use fnmatch(3)
     * to do the matching.
     */
    if (has_meta(sudoers_cmnd)) {
	/*
	 * Return true if we find a match in the glob(3) results AND
	 *  a) there are no args in sudoers OR
	 *  b) there are no args on command line and none required by sudoers OR
	 *  c) there are args in sudoers and on command line and they match
	 * else return false.
	 */
#define GLOB_FLAGS	(GLOB_NOSORT | GLOB_MARK | GLOB_BRACE | GLOB_TILDE)
	if (glob(sudoers_cmnd, GLOB_FLAGS, NULL, &gl) != 0) {
	    globfree(&gl);
	    return(FALSE);
	}
	/* For each glob match, compare basename, st_dev and st_ino. */
	for (ap = gl.gl_pathv; *ap != NULL; ap++) {
	    /* only stat if basenames are the same */
	    if ((base = strrchr(*ap, '/')) != NULL)
		base++;
	    else
		base = *ap;
	    if (strcmp(user_base, base) != 0 ||
		stat(*ap, &sudoers_stat) == -1)
		continue;
	    if (user_stat->st_dev == sudoers_stat.st_dev &&
		user_stat->st_ino == sudoers_stat.st_ino) {
		if (safe_cmnd)
		    free(safe_cmnd);
		safe_cmnd = estrdup(*ap);
		break;
	    }
	}
	globfree(&gl);
	if (*ap == NULL)
	    return(FALSE);

	if (!sudoers_args ||
	    (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
	    (sudoers_args &&
	     fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
	    if (safe_cmnd)
		free(safe_cmnd);
	    safe_cmnd = estrdup(user_cmnd);
	    return(TRUE);
	} else
	    return(FALSE);
    } else {
	size_t dlen = strlen(sudoers_cmnd);

	/*
	 * No meta characters
	 * Check to make sure this is not a directory spec (doesn't end in '/')
	 */
	if (sudoers_cmnd[dlen - 1] != '/') {
	    /* Only proceed if user_base and basename(sudoers_cmnd) match */
	    if ((base = strrchr(sudoers_cmnd, '/')) == NULL)
		base = sudoers_cmnd;
	    else
		base++;
	    if (strcmp(user_base, base) != 0 ||
		stat(sudoers_cmnd, &sudoers_stat) == -1)
		return(FALSE);

	    /*
	     * Return true if inode/device matches AND
	     *  a) there are no args in sudoers OR
	     *  b) there are no args on command line and none req by sudoers OR
	     *  c) there are args in sudoers and on command line and they match
	     */
	    if (user_stat->st_dev != sudoers_stat.st_dev ||
		user_stat->st_ino != sudoers_stat.st_ino)
		return(FALSE);
	    if (!sudoers_args ||
		(!user_args && sudoers_args && !strcmp("\"\"", sudoers_args)) ||
		(sudoers_args &&
		 fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0)) {
		if (safe_cmnd)
		    free(safe_cmnd);
		safe_cmnd = estrdup(sudoers_cmnd);
		return(TRUE);
	    } else
		return(FALSE);
	}

	/*
	 * Grot through sudoers_cmnd's directory entries, looking for user_base.
	 */
	dirp = opendir(sudoers_cmnd);
	if (dirp == NULL)
	    return(FALSE);

	if (strlcpy(buf, sudoers_cmnd, sizeof(buf)) >= sizeof(buf))
	    return(FALSE);
	while ((dent = readdir(dirp)) != NULL) {
	    /* ignore paths > PATH_MAX (XXX - log) */
	    buf[dlen] = '\0';
	    if (strlcat(buf, dent->d_name, sizeof(buf)) >= sizeof(buf))
		continue;

	    /* only stat if basenames are the same */
	    if (strcmp(user_base, dent->d_name) != 0 ||
		stat(buf, &sudoers_stat) == -1)
		continue;
	    if (user_stat->st_dev == sudoers_stat.st_dev &&
		user_stat->st_ino == sudoers_stat.st_ino) {
		if (safe_cmnd)
		    free(safe_cmnd);
		safe_cmnd = estrdup(buf);
		break;
	    }
	}

	closedir(dirp);
	return(dent != NULL);
    }
}

/*
 * Returns TRUE if "n" is one of our ip addresses or if
 * "n" is a network that we are on, else returns FALSE.
 */
int
addr_matches(n)
    char *n;
{
    int i;
    char *m;
    struct in_addr addr, mask;

    /* If there's an explicit netmask, use it. */
    if ((m = strchr(n, '/'))) {
	*m++ = '\0';
	addr.s_addr = inet_addr(n);
	if (strchr(m, '.'))
	    mask.s_addr = inet_addr(m);
	else {
	    i = 32 - atoi(m);
	    mask.s_addr = 0xffffffff;
	    mask.s_addr >>= i;
	    mask.s_addr <<= i;
	    mask.s_addr = htonl(mask.s_addr);
	}
	*(m - 1) = '/';

	for (i = 0; i < num_interfaces; i++)
	    if ((interfaces[i].addr.s_addr & mask.s_addr) == addr.s_addr)
		return(TRUE);
    } else {
	addr.s_addr = inet_addr(n);

	for (i = 0; i < num_interfaces; i++)
	    if (interfaces[i].addr.s_addr == addr.s_addr ||
		(interfaces[i].addr.s_addr & interfaces[i].netmask.s_addr)
		== addr.s_addr)
		return(TRUE);
    }

    return(FALSE);
}

/*
 * Returns 0 if the hostname matches the pattern and non-zero otherwise.
 */
int
hostname_matches(shost, lhost, pattern)
    char *shost;
    char *lhost;
    char *pattern;
{
    if (has_meta(pattern)) {
	if (strchr(pattern, '.'))
	    return(fnmatch(pattern, lhost, FNM_CASEFOLD));
	else
	    return(fnmatch(pattern, shost, FNM_CASEFOLD));
    } else {
	if (strchr(pattern, '.'))
	    return(strcasecmp(lhost, pattern));
	else
	    return(strcasecmp(shost, pattern));
    }
}

/*
 *  Returns TRUE if the user/uid from sudoers matches the specified user/uid,
 *  else returns FALSE.
 */
int
userpw_matches(sudoers_user, user, pw)
    char *sudoers_user;
    char *user;
    struct passwd *pw;
{
    if (pw != NULL && *sudoers_user == '#') {
	uid_t uid = (uid_t) atoi(sudoers_user + 1);
	if (uid == pw->pw_uid)
	    return(1);
    }
    return(strcmp(sudoers_user, user) == 0);
}

/*
 *  Returns TRUE if the given user belongs to the named group,
 *  else returns FALSE.
 *  XXX - reduce the number of passwd/group lookups
 */
int
usergr_matches(group, user, pw)
    char *group;
    char *user;
    struct passwd *pw;
{
    struct group *grp;
    gid_t pw_gid;
    char **cur;

    /* make sure we have a valid usergroup, sudo style */
    if (*group++ != '%')
	return(FALSE);

    /* look up user's primary gid in the passwd file */
    if (pw == NULL && (pw = getpwnam(user)) == NULL)
	return(FALSE);
    pw_gid = pw->pw_gid;

    if ((grp = getgrnam(group)) == NULL)
	return(FALSE);

    /* check against user's primary (passwd file) gid */
    if (grp->gr_gid == pw_gid)
	return(TRUE);

    /* check to see if user is explicitly listed in the group */
    for (cur = grp->gr_mem; *cur; cur++) {
	if (strcmp(*cur, user) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

/*
 * Returns TRUE if "host" and "user" belong to the netgroup "netgr",
 * else return FALSE.  Either of "host", "shost" or "user" may be NULL
 * in which case that argument is not checked...
 *
 * XXX - swap order of host & shost
 */
int
netgr_matches(netgr, lhost, shost, user)
    char *netgr;
    char *lhost;
    char *shost;
    char *user;
{
#ifdef HAVE_GETDOMAINNAME
    static char *domain = (char *) -1;
#else
    static char *domain = NULL;
#endif /* HAVE_GETDOMAINNAME */

    /* make sure we have a valid netgroup, sudo style */
    if (*netgr++ != '+')
	return(FALSE);

#ifdef HAVE_GETDOMAINNAME
    /* get the domain name (if any) */
    if (domain == (char *) -1) {
	domain = (char *) emalloc(MAXHOSTNAMELEN);
	if (getdomainname(domain, MAXHOSTNAMELEN) == -1 || *domain == '\0') {
	    free(domain);
	    domain = NULL;
	}
    }
#endif /* HAVE_GETDOMAINNAME */

#ifdef HAVE_INNETGR
    if (innetgr(netgr, lhost, user, domain))
	return(TRUE);
    else if (lhost != shost && innetgr(netgr, shost, user, domain))
	return(TRUE);
#endif /* HAVE_INNETGR */

    return(FALSE);
}

/*
 * Returns TRUE if "s" has shell meta characters in it,
 * else returns FALSE.
 */
static int
has_meta(s)
    char *s;
{
    char *t;
 
    for (t = s; *t; t++) {
	if (*t == '\\' || *t == '?' || *t == '*' || *t == '[' || *t == ']')
	    return(TRUE);
    }
    return(FALSE);
}
