/*
* Dirent interface for Microsoft Visual Studio
* Version 1.21
*
* Copyright (C) 2006-2012 Toni Ronkko
* This file is part of dirent.  Dirent may be freely distributed
* under the MIT license.  For all details and documentation, see
* https://github.com/tronkko/dirent
*/

#include <stdio.h>
#include <stdarg.h>
#include <windows.h>
#include <winbase.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "dirent.h"


_WDIR* _wopendir(const wchar_t* dirname)
{
	_WDIR *dirp = NULL;
	int error;

	/* Must have directory name */
	if (dirname == NULL || dirname[0] == '\0') {
		dirent_set_errno(ENOENT);
		return NULL;
	}

	/* Allocate new _WDIR structure */
	dirp = (_WDIR*)malloc(sizeof(struct _WDIR));
	if (dirp != NULL) {
		DWORD n;

		/* Reset _WDIR structure */
		dirp->handle = INVALID_HANDLE_VALUE;
		dirp->patt = NULL;
		dirp->cached = 0;

		/* Compute the length of full path plus zero terminator */
		n = GetFullPathNameW(dirname, 0, NULL, NULL);

		/* Allocate room for absolute directory name and search pattern */
		dirp->patt = (wchar_t*)malloc(sizeof(wchar_t) * n + 16);
		if (dirp->patt) {

			/*
			* Convert relative directory name to an absolute one.  This
			* allows rewinddir() to function correctly even when current
			* working directory is changed between opendir() and rewinddir().
			*/
			n = GetFullPathNameW(dirname, n, dirp->patt, NULL);
			if (n > 0) {
				wchar_t *p;

				/* Append search pattern \* to the directory name */
				p = dirp->patt + n;
				if (dirp->patt < p) {
					switch (p[-1]) {
					case '\\':
					case '/':
					case ':':
						/* Directory ends in path separator, e.g. c:\temp\ */
						/*NOP*/;
						break;

					default:
						/* Directory name doesn't end in path separator */
						*p++ = '\\';
					}
				}
				*p++ = '*';
				*p = '\0';

				/* Open directory stream and retrieve the first entry */
				if (dirent_first(dirp)) {
					/* Directory stream opened successfully */
					error = 0;
				}
				else {
					/* Cannot retrieve first entry */
					error = 1;
					dirent_set_errno(ENOENT);
				}

			}
			else {
				/* Cannot retrieve full path name */
				dirent_set_errno(ENOENT);
				error = 1;
			}

		}
		else {
			/* Cannot allocate memory for search pattern */
			error = 1;
		}

	}
	else {
		/* Cannot allocate _WDIR structure */
		error = 1;
	}

	/* Clean up in case of error */
	if (error  &&  dirp) {
		_wclosedir(dirp);
		dirp = NULL;
	}

	return dirp;
}

struct _wdirent* _wreaddir(_WDIR* dirp)
{
	WIN32_FIND_DATAW *datap;
	struct _wdirent *entp;

	/* Read next directory entry */
	datap = dirent_next(dirp);
	if (datap) {
		size_t n;
		DWORD attr;

		/* Pointer to directory entry to return */
		entp = &dirp->ent;

		/*
		* Copy file name as wide-character string.  If the file name is too
		* long to fit in to the destination buffer, then truncate file name
		* to PATH_MAX characters and zero-terminate the buffer.
		*/
		n = 0;
		while (n + 1 < PATH_MAX  &&  datap->cFileName[n] != 0) {
			entp->d_name[n] = datap->cFileName[n];
			n++;
		}
		dirp->ent.d_name[n] = 0;

		/* Length of file name excluding zero terminator */
		entp->d_namlen = n;

		/* File type */
		attr = datap->dwFileAttributes;
		if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
			entp->d_type = DT_CHR;
		}
		else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			entp->d_type = DT_DIR;
		}
		else {
			entp->d_type = DT_REG;
		}

		/* Reset dummy fields */
		entp->d_ino = 0;
		entp->d_reclen = sizeof(struct _wdirent);

	}
	else {

		/* Last directory entry read */
		entp = NULL;

	}

	return entp;
}

int _wclosedir(_WDIR* dirp)
{
	int ok;
	if (dirp) {

		/* Release search handle */
		if (dirp->handle != INVALID_HANDLE_VALUE) {
			FindClose(dirp->handle);
			dirp->handle = INVALID_HANDLE_VALUE;
		}

		/* Release search pattern */
		if (dirp->patt) {
			free(dirp->patt);
			dirp->patt = NULL;
		}

		/* Release directory structure */
		free(dirp);
		ok = /*success*/0;

	}
	else {
		/* Invalid directory stream */
		dirent_set_errno(EBADF);
		ok = /*failure*/-1;
	}
	return ok;
}

void _wrewinddir(_WDIR* dirp)
{
	if (dirp) {
		/* Release existing search handle */
		if (dirp->handle != INVALID_HANDLE_VALUE) {
			FindClose(dirp->handle);
		}

		/* Open new search handle */
		dirent_first(dirp);
	}
}

WIN32_FIND_DATAW* dirent_first(_WDIR* dirp)
{
	WIN32_FIND_DATAW *datap;

	/* Open directory and retrieve the first entry */
	dirp->handle = FindFirstFileW(dirp->patt, &dirp->data);
	if (dirp->handle != INVALID_HANDLE_VALUE) {

		/* a directory entry is now waiting in memory */
		datap = &dirp->data;
		dirp->cached = 1;

	}
	else {

		/* Failed to re-open directory: no directory entry in memory */
		dirp->cached = 0;
		datap = NULL;

	}
	return datap;
}

WIN32_FIND_DATAW* dirent_next(_WDIR* dirp)
{
	WIN32_FIND_DATAW *p;

	/* Get next directory entry */
	if (dirp->cached != 0) {

		/* A valid directory entry already in memory */
		p = &dirp->data;
		dirp->cached = 0;

	}
	else if (dirp->handle != INVALID_HANDLE_VALUE) {

		/* Get the next directory entry from stream */
		if (FindNextFileW(dirp->handle, &dirp->data) != FALSE) {
			/* Got a file */
			p = &dirp->data;
		}
		else {
			/* The very last entry has been processed or an error occured */
			FindClose(dirp->handle);
			dirp->handle = INVALID_HANDLE_VALUE;
			p = NULL;
		}

	}
	else {

		/* End of directory stream reached */
		p = NULL;

	}

	return p;
}

DIR* opendir(const char* dirname)
{
	struct DIR *dirp;
	int error;

	/* Must have directory name */
	if (dirname == NULL || dirname[0] == '\0') {
		dirent_set_errno(ENOENT);
		return NULL;
	}

	/* Allocate memory for DIR structure */
	dirp = (DIR*)malloc(sizeof(struct DIR));
	if (dirp) {
		wchar_t wname[PATH_MAX];
		size_t n;

		/* Convert directory name to wide-character string */
		error = dirent_mbstowcs_s(&n, wname, PATH_MAX, dirname, PATH_MAX);
		if (!error) {

			/* Open directory stream using wide-character name */
			dirp->wdirp = _wopendir(wname);
			if (dirp->wdirp) {
				/* Directory stream opened */
				error = 0;
			}
			else {
				/* Failed to open directory stream */
				error = 1;
			}

		}
		else {
			/*
			* Cannot convert file name to wide-character string.  This
			* occurs if the string contains invalid multi-byte sequences or
			* the output buffer is too small to contain the resulting
			* string.
			*/
			error = 1;
		}

	}
	else {
		/* Cannot allocate DIR structure */
		error = 1;
	}

	/* Clean up in case of error */
	if (error  &&  dirp) {
		free(dirp);
		dirp = NULL;
	}

	return dirp;
}

struct dirent* readdir(DIR* dirp)
{
	WIN32_FIND_DATAW *datap;
	struct dirent *entp;

	/* Read next directory entry */
	datap = dirent_next(dirp->wdirp);
	if (datap) {
		size_t n;
		int error;

		/* Attempt to convert file name to multi-byte string */
		error = dirent_wcstombs_s(
			&n, dirp->ent.d_name, PATH_MAX, datap->cFileName, PATH_MAX);

		/*
		* If the file name cannot be represented by a multi-byte string,
		* then attempt to use old 8+3 file name.  This allows traditional
		* Unix-code to access some file names despite of unicode
		* characters, although file names may seem unfamiliar to the user.
		*
		* Be ware that the code below cannot come up with a short file
		* name unless the file system provides one.  At least
		* VirtualBox shared folders fail to do this.
		*/
		if (error  &&  datap->cAlternateFileName[0] != '\0') {
			error = dirent_wcstombs_s(
				&n, dirp->ent.d_name, PATH_MAX,
				datap->cAlternateFileName, PATH_MAX);
		}

		if (!error) {
			DWORD attr;

			/* Initialize directory entry for return */
			entp = &dirp->ent;

			/* Length of file name excluding zero terminator */
			entp->d_namlen = n - 1;

			/* File attributes */
			attr = datap->dwFileAttributes;
			if ((attr & FILE_ATTRIBUTE_DEVICE) != 0) {
				entp->d_type = DT_CHR;
			}
			else if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				entp->d_type = DT_DIR;
			}
			else {
				entp->d_type = DT_REG;
			}

			/* Reset dummy fields */
			entp->d_ino = 0;
			entp->d_reclen = sizeof(struct dirent);

		}
		else {
			/*
			* Cannot convert file name to multi-byte string so construct
			* an errornous directory entry and return that.  Note that
			* we cannot return NULL as that would stop the processing
			* of directory entries completely.
			*/
			entp = &dirp->ent;
			entp->d_name[0] = '?';
			entp->d_name[1] = '\0';
			entp->d_namlen = 1;
			entp->d_type = DT_UNKNOWN;
			entp->d_ino = 0;
			entp->d_reclen = 0;
		}

	}
	else {
		/* No more directory entries */
		entp = NULL;
	}

	return entp;
}

int closedir(DIR* dirp)
{
	int ok;
	if (dirp) {

		/* Close wide-character directory stream */
		ok = _wclosedir(dirp->wdirp);
		dirp->wdirp = NULL;

		/* Release multi-byte character version */
		free(dirp);

	}
	else {

		/* Invalid directory stream */
		dirent_set_errno(EBADF);
		ok = /*failure*/-1;

	}
	return ok;
}

void rewinddir(DIR* dirp)
{
	/* Rewind wide-character string directory stream */
	_wrewinddir(dirp->wdirp);
}

int dirent_mbstowcs_s(
	size_t* pReturnValue,
	wchar_t* wcstr,
	size_t sizeInWords,
	const char* mbstr,
	size_t count)
{
	int error;

#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

	/* Microsoft Visual Studio 2005 or later */
	error = mbstowcs_s(pReturnValue, wcstr, sizeInWords, mbstr, count);

#else

	/* Older Visual Studio or non-Microsoft compiler */
	size_t n;

	/* Convert to wide-character string (or count characters) */
	n = mbstowcs(wcstr, mbstr, sizeInWords);
	if (!wcstr || n < count) {

		/* Zero-terminate output buffer */
		if (wcstr  &&  sizeInWords) {
			if (n >= sizeInWords) {
				n = sizeInWords - 1;
			}
			wcstr[n] = 0;
		}

		/* Length of resuting multi-byte string WITH zero terminator */
		if (pReturnValue) {
			*pReturnValue = n + 1;
		}

		/* Success */
		error = 0;

	}
	else {

		/* Could not convert string */
		error = 1;

	}

#endif

	return error;
}

int dirent_wcstombs_s(
	size_t* pReturnValue,
	char* mbstr,
	size_t sizeInBytes, /* max size of mbstr */
	const wchar_t* wcstr,
	size_t count)
{
	int error;

#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

	/* Microsoft Visual Studio 2005 or later */
	error = wcstombs_s(pReturnValue, mbstr, sizeInBytes, wcstr, count);

#else

	/* Older Visual Studio or non-Microsoft compiler */
	size_t n;

	/* Convert to multi-byte string (or count the number of bytes needed) */
	n = wcstombs(mbstr, wcstr, sizeInBytes);
	if (!mbstr || n < count) {

		/* Zero-terminate output buffer */
		if (mbstr  &&  sizeInBytes) {
			if (n >= sizeInBytes) {
				n = sizeInBytes - 1;
			}
			mbstr[n] = '\0';
		}

		/* Lenght of resulting multi-bytes string WITH zero-terminator */
		if (pReturnValue) {
			*pReturnValue = n + 1;
		}

		/* Success */
		error = 0;

	}
	else {

		/* Cannot convert string */
		error = 1;

	}

#endif

	return error;
}

void dirent_set_errno(int error)
{
#if defined(_MSC_VER)  &&  _MSC_VER >= 1400

	/* Microsoft Visual Studio 2005 and later */
	_set_errno(error);

#else

	/* Non-Microsoft compiler or older Microsoft compiler */
	errno = error;

#endif
}

