/* Formerly from xetexextra.c: */

#define EXTERN
#include <xetexd.h>

/* texmfmp.c: Hand-coded routines for TeX or Metafont in C.  Originally
   written by Tim Morgan, drawing from other Unix ports of TeX.  This is
   a collection of miscellany, everything that's easier (or only
   possible) to do in C.

   This file is public domain.  */

/* This file is included from, e.g., texextra,c after
      #define EXTERN
      #include <texd.h>
   to instantiate data from texd.h here.  The ?d.h file is what
   #defines TeX or MF, which avoids the need for a special
   Makefile rule.  */

#include <tidy_kpathutil.h>
#include <kpsezip/public.h>

#include <sys/time.h>
#include <time.h> /* For `struct tm'.  Moved here for Visual Studio 2005.  */
#include <locale.h>

#include <signal.h> /* Catch interrupts.  */

#ifndef strnumber
#define strnumber str_number
#endif

/* formerly texmfmp-help.h: */

const_string XETEXHELP[] = {
    "Usage: xetex [OPTION]... [TEXNAME[.tex]] [COMMANDS]",
    "   or: xetex [OPTION]... \\FIRST-LINE",
    "   or: xetex [OPTION]... &FMT ARGS",
    "  Run XeTeX on TEXNAME, usually creating TEXNAME.pdf.",
    "  Any remaining COMMANDS are processed as XeTeX input, after TEXNAME is read.",
    "  If the first line of TEXNAME is %&FMT, and FMT is an existing .fmt file,",
    "  use it.  Else use `NAME.fmt', where NAME is the program invocation name,",
    "  most commonly `xetex'.",
    "",
    "  Alternatively, if the first non-option argument begins with a backslash,",
    "  interpret all non-option arguments as a line of XeTeX input.",
    "",
    "  Alternatively, if the first non-option argument begins with a &, the",
    "  next word is taken as the FMT to read, overriding all else.  Any",
    "  remaining arguments are processed as above.",
    "",
    "  If no arguments or options are specified, prompt for input.",
    "",
    "-etex                   enable e-TeX extensions",
    "[-no]-file-line-error   disable/enable file:line:error style messages",
    "-fmt=FMTNAME            use FMTNAME instead of program name or a %& line",
    "-halt-on-error          stop processing at the first error",
    "-ini                    be xeinitex, for dumping formats; this is implicitly",
    "                          true if the program name is `xeinitex'",
    "-interaction=STRING     set interaction mode (STRING=batchmode/nonstopmode/",
    "                          scrollmode/errorstopmode)",
    "-jobname=STRING         set the job name to STRING",
    "-kpathsea-debug=NUMBER  set path searching debugging flags according to",
    "                          the bits of NUMBER",
    "[-no]-mktex=FMT         disable/enable mktexFMT generation (FMT=tex/tfm)",
    "-mltex                  enable MLTeX extensions such as \\charsubdef",
    "-output-comment=STRING  use STRING for XDV file comment instead of date",
    "-output-directory=DIR   use existing DIR as the directory to write files in",
    "-output-driver=CMD      use CMD as the XDV-to-PDF driver instead of xdvipdfmx",
    "-no-pdf                 generate XDV (extended DVI) output rather than PDF",
    "[-no]-parse-first-line  disable/enable parsing of first line of input file",
    "-papersize=STRING       set PDF media size to STRING",
    "-progname=STRING        set program (and fmt) name to STRING",
    "-recorder               enable filename recorder",
    "[-no]-shell-escape      disable/enable \\write18{SHELL COMMAND}",
    "-shell-restricted       enable restricted \\write18",
    "-src-specials           insert source specials into the XDV file",
    "-src-specials=WHERE     insert source specials in certain places of",
    "                          the XDV file. WHERE is a comma-separated value",
    "                          list: cr display hbox math par parend vbox",
    "-synctex=NUMBER         generate SyncTeX data for previewers if nonzero",
    "-translate-file=TCXNAME (ignored)",
    "-8bit                   make all characters printable, don't use ^^X sequences",
    "-help                   display this help and exit",
    "-version                output version information and exit",
    NULL
};

/* end texmfmp-help.h */

/* {tex,mf}d.h defines TeX, MF, INI, and other such symbols.
   Unfortunately there's no way to get the banner into this code, so
   just repeat the text.  */
/* We also define predicates, e.g., IS_eTeX for all e-TeX like engines, so
   the rest of this file can remain unchanged when adding a new engine.  */

/* formerly xetexextra.h: */

#define ETEX_VERSION "2.6"
#define XETEX_VERSION "0.99996"
#define BANNER "This is XeTeX, Version 3.14159265-" ETEX_VERSION "-" XETEX_VERSION
#define COPYRIGHT_HOLDER "SIL International, Jonathan Kew and Khaled Hosny"
#define AUTHOR "Jonathan Kew"
#define BUG_ADDRESS "xetex@tug.org"
#define DUMP_VAR TEX_format_default
#define DUMP_LENGTH_VAR format_default_length
#define DUMP_OPTION "fmt"
#define DUMP_EXT ".fmt"
#define INI_PROGRAM "xeinitex"
#define VIR_PROGRAM "xevirtex"

/* end xetexextra.h */

#if !defined(IS_eTeX)
# define IS_eTeX 0
#endif
#if !defined(IS_pTeX)
# define IS_pTeX 0
#endif
#if !defined(IS_upTeX)
# define IS_upTeX 0
#endif

/*
   SyncTeX file name should be full path in the case where
   --output-directory option is given.
   Borrowed from LuaTeX.
*/
char *generic_synctex_get_current_name (void)
{
  char *pwdbuf, *ret;
  if (!fullnameoffile) {
    ret = xstrdup("");
    return ret;
  }
  if (kpse_absolute_p(fullnameoffile, false)) {
     return xstrdup(fullnameoffile);
  }
  pwdbuf = xgetcwd();
  ret = concat3(pwdbuf, DIR_SEP_STRING, fullnameoffile);
  free(pwdbuf) ;
  return ret;
}

/* Defused shell escape. */

static void
init_shell_escape (void)
{
  if (shellenabledp < 0) {  /* --no-shell-escape on cmd line */
    shellenabledp = 0;
  }
}

int
runsystem (const char *cmd)
{
    return 0;
}

#if ENABLE_PIPES
static FILE *
runpopen (char *cmd, const char *mode)
{
    return NULL;
}
#endif /* ENABLE_PIPES */

/* The main program, etc.  */

#include "XeTeX_ext.h"

/* What we were invoked as and with.  */
char **argv;
int argc;

/* If the user overrides argv[0] with -progname.  */
static const_string user_progname;

/* The C version of the jobname, if given. */
static const_string c_job_name;

/* The filename for dynamic character translation, or NULL.  */
string translate_filename;
string default_translate_filename;

/* Needed for --src-specials option. */
static char *last_source_name;
static int last_lineno;
static boolean src_specials_option = false;
static void parse_src_specials_option (const_string);

/* Parsing a first %&-line in the input file. */
static void parse_first_line (const_string);

/* Parse option flags. */
static void parse_options (int, string *);

/* Try to figure out if we have been given a filename. */
static string get_input_file_name (void);

/* Get a true/false value for a variable from texmf.cnf and the environment. */
static boolean
texmf_yesno(const_string var)
{
    return 0;
}

/* The entry point: set up for reading the command line, which will
   happen in `t_open_in', then call the main body.  */

void
maininit (int ac, string *av)
{
  string main_input_file;
  /* Save to pass along to t_open_in.  */
  argc = ac;
  argv = av;

  /* Must be initialized before options are parsed.  */
  interaction_option = 4;

  /* [The "recorder" input and output functions used to be set here.] */

  /* 0 means "disable Synchronize TeXnology".
     synctexoption is a *.web variable.
     We initialize it to a weird value to catch the -synctex command line flag.
     At runtime, if synctexoption is not INT_MAX, then it contains the
     command line option provided; otherwise, no such option was given
     by the user.  */
# define SYNCTEX_NO_OPTION INT_MAX
  synctexoption = SYNCTEX_NO_OPTION;

  /* If the user says --help or --version, we need to notice early.  And
     since we want the --ini option, have to do it before getting into
     the web (which would read the base file, etc.).  */
  parse_options (ac, av);

  /* If -progname was not specified, default to the dump name.  */
  if (!user_progname)
    user_progname = dump_name;

  /* Do this early so we can inspect kpse_invocation_name and
     kpse_program_name below, and because we have to do this before
     any path searching.  */

  /* FIXME: gather engine names in a single spot. */
  xputenv ("engine", TEXMF_ENGINE_NAME);

  /* Were we given a simple filename? */
  main_input_file = get_input_file_name();

  /* Second chance to activate file:line:error style messages, this
     time from texmf.cnf. */
  if (file_line_error_style_p < 0) {
    file_line_error_style_p = 0;
  } else if (!file_line_error_style_p) {
    file_line_error_style_p = texmf_yesno ("file_line_error_style");
  }

  /* If no dump default yet, and we're not doing anything special on
     this run, we may want to look at the first line of the main input
     file for a %&<dumpname> specifier.  */
  if (parse_first_line_p < 0) {
    parse_first_line_p = 0;
  } else if (!parse_first_line_p) {
    parse_first_line_p = texmf_yesno ("parse_first_line");
  }
  if (parse_first_line_p && (!dump_name || !translate_filename)) {
    parse_first_line (main_input_file);
  }
  /* Check whether there still is no translate_filename known.  If so,
     use the default_translate_filename. */
  /* FIXME: deprecated. */
  if (!translate_filename) {
    translate_filename = default_translate_filename;
  }
  /* If we're preloaded, I guess everything is set up.  I don't really
     know any more, it's been so long since anyone preloaded.  */
  if (ready_already != 314159) {
    if (!dump_name)
	dump_name = "xelatex";
  }

  /* Sanity check: -mltex, -enc, -etex only work in combination with -ini. */
  if (!ini_version) {
    if (mltex_p) {
      fprintf(stderr, "-mltex only works with -ini\n");
    }
  }

  /* If we've set up the fmt/base default in any of the various ways
     above, also set its length.  */
  if (dump_name) {
    const_string with_ext = NULL;
    unsigned name_len = strlen (dump_name);
    unsigned ext_len = strlen (DUMP_EXT);

    /* Provide extension if not there already.  */
    if (name_len > ext_len
        && FILESTRCASEEQ (dump_name + name_len - ext_len, DUMP_EXT)) {
      with_ext = dump_name;
    } else {
      with_ext = concat (dump_name, DUMP_EXT);
    }
    DUMP_VAR = concat (" ", with_ext); /* adjust array for Pascal */
    DUMP_LENGTH_VAR = strlen (DUMP_VAR + 1);
  } else {
    /* For dump_name to be NULL is a bug.  */
    abort();
  }

  init_shell_escape ();
}

/* The entry point: set up for reading the command line, which will
   happen in `t_open_in', then call the main body.  */

int
main (int ac, string *av)
{
  maininit (ac, av);

  /* Call the real main program.  */
  main_body ();

  return EXIT_SUCCESS;
}

/* This is supposed to ``open the terminal for input'', but what we
   really do is copy command line arguments into TeX's or Metafont's
   buffer, so they can handle them.  If nothing is available, or we've
   been called already (and hence, argc==0), we return with
   `last=first'.  */

void
t_open_in (void)
{
  int i;

  static UFILE termin_file;
  if (term_in == 0) {
    term_in = &termin_file;
    term_in->f = stdin;
    term_in->savedChar = -1;
    term_in->skipNextLF = 0;
    term_in->encodingMode = UTF8;
    term_in->conversionData = 0;
    input_file[0] = term_in;
  }

  buffer[first] = 0; /* In case there are no arguments.  */

  if (optind < argc) { /* We have command line arguments.  */
    int k = first;
    for (i = optind; i < argc; i++) {
      unsigned char *ptr = (unsigned char *)&(argv[i][0]);
      /* need to interpret UTF8 from the command line */
      UInt32 rval;
      while ((rval = *(ptr++)) != 0) {
        UInt16 extraBytes = bytesFromUTF8[rval];
        switch (extraBytes) { /* note: code falls through cases! */
          case 5: rval <<= 6; if (*ptr) rval += *(ptr++);
          case 4: rval <<= 6; if (*ptr) rval += *(ptr++);
          case 3: rval <<= 6; if (*ptr) rval += *(ptr++);
          case 2: rval <<= 6; if (*ptr) rval += *(ptr++);
          case 1: rval <<= 6; if (*ptr) rval += *(ptr++);
          case 0: ;
        };
        rval -= offsetsFromUTF8[extraBytes];
        buffer[k++] = rval;
      }
      buffer[k++] = ' ';
    }
    argc = 0;	/* Don't do this again.  */
    buffer[k] = 0;
  }

  /* Find the end of the buffer.  */
  for (last = first; buffer[last]; ++last)
    ;

  /* Make `last' be one past the last non-blank character in `buffer'.  */
  /* ??? The test for '\r' should not be necessary.  */
  for (--last; last >= first
       && ISBLANK (buffer[last]) && buffer[last] != '\r'; --last)
    ;
  last++;
}

#if defined (TeX) || defined (MF)
  /* TCX and Aleph&Co get along like sparks and gunpowder. */
#if !defined(Aleph) && !defined(XeTeX)

/* Return the next number following START, setting POST to the following
   character, as in strtol.  Issue a warning and return -1 if no number
   can be parsed.  */

static int
tcx_get_num (int upb,
             unsigned line_count,
             string start,
             string *post)
{
  int num = strtol (start, post, 0);
  assert (post && *post);
  if (*post == start) {
    /* Could not get a number. If blank line, fine. Else complain.  */
    string p = start;
    while (*p && ISSPACE (*p))
      p++;
    if (*p != 0)
      fprintf (stderr, "%s:%d: Expected numeric constant, not `%s'.\n",
               translate_filename, line_count, start);
    num = -1;
  } else if (num < 0 || num > upb) {
    fprintf (stderr, "%s:%d: Destination charcode %d <0 or >%d.\n",
             translate_filename, line_count, num, upb);
    num = -1;
  }

  return num;
}

/* Update the xchr, xord, and xprn arrays for TeX, allowing a
   translation table specified at runtime via an external file.
   Look for the character translation file FNAME along the same path as
   tex.pool.  If no suffix in FNAME, use .tcx (don't bother trying to
   support extension-less names for these files).  */

/* FIXME: A new format ought to be introduced for these files. */

void
readtcxfile (void)
{
  string orig_filename;
  if (!find_suffix (translate_filename)) {
    translate_filename = concat (translate_filename, ".tcx");
  }
  orig_filename = translate_filename;
  translate_filename
    = kpse_find_file (translate_filename, kpse_web2c_format, true);
  if (translate_filename) {
    string line;
    unsigned line_count = 0;
    FILE *translate_file = xfopen (translate_filename, FOPEN_R_MODE);
    while ((line = read_line (translate_file))) {
      int first;
      string start2;
      string comment_loc = strchr (line, '%');
      if (comment_loc)
        *comment_loc = 0;

      line_count++;

      first = tcx_get_num (255, line_count, line, &start2);
      if (first >= 0) {
        string start3;
        int second;
        int printable;

        second = tcx_get_num (255, line_count, start2, &start3);
        if (second >= 0) {
            /* I suppose we could check for nonempty junk following the
               "printable" code, but let's not bother.  */
          string extra;

          /* If they mention a second code, make that the internal number.  */
          xord[first] = second;
          xchr[second] = first;

          printable = tcx_get_num (1, line_count, start3, &extra);
          /* Not-a-number, may be a comment. */
          if (printable == -1)
            printable = 1;
          /* Don't allow the 7bit ASCII set to become unprintable. */
          if (32 <= second && second <= 126)
            printable = 1;
        } else {
          second = first; /* else make internal the same as external */
          /* If they mention a charcode, call it printable.  */
          printable = 1;
        }

        xprn[second] = printable;
      }
      free (line);
    }
    xfclose(translate_file, translate_filename);
  } else {
    WARNING1 ("Could not open char translation file `%s'", orig_filename);
  }
}
#endif /* !Aleph && !XeTeX */
#endif /* TeX || MF [character translation] */

#ifdef XeTeX /* XeTeX handles this differently, and allows odd quotes within names */
static string
normalize_quotes (const_string name, const_string mesg)
{
    int quote_char = 0;
    boolean must_quote = false;
    int len = strlen(name);
    /* Leave room for quotes and NUL. */
    string ret;
    string p;
    const_string q;
    for (q = name; *q; q++) {
        if (*q == ' ') {
            if (!must_quote) {
                len += 2;
                must_quote = true;
            }
        }
        else if (*q == '\"' || *q == '\'') {
            must_quote = true;
            if (quote_char == 0)
                quote_char = '\"' + '\'' - *q;
            len += 2; /* this could sometimes add length we don't need */
        }
    }
    ret = xmalloc(len + 1);
    p = ret;
    if (must_quote) {
        if (quote_char == 0)
            quote_char = '\"';
        *p++ = quote_char;
    }
    for (q = name; *q; q++) {
        if (*q == quote_char) {
            *p++ = quote_char;
            quote_char = '\"' + '\'' - quote_char;
            *p++ = quote_char;
        }
        *p++ = *q;
    }
    if (quote_char != 0)
        *p++ = quote_char;
    *p = '\0';
    return ret;
}
#else
/* Normalize quoting of filename -- that is, only quote if there is a space,
   and always use the quote-name-quote style. */
static string
normalize_quotes (const_string name, const_string mesg)
{
    boolean quoted = false;
    boolean must_quote = (strchr(name, ' ') != NULL);
    /* Leave room for quotes and NUL. */
    string ret = xmalloc(strlen(name)+3);
    string p;
    const_string q;
    p = ret;
    if (must_quote)
        *p++ = '"';
    for (q = name; *q; q++) {
        if (*q == '"')
            quoted = !quoted;
        else
            *p++ = *q;
    }
    if (must_quote)
        *p++ = '"';
    *p = '\0';
    if (quoted) {
        fprintf(stderr, "! Unbalanced quotes in %s %s\n", mesg, name);
        uexit(1);
    }
    return ret;
}
#endif

/* Getting the input filename. */
string
get_input_file_name (void)
{
    string input_file_name = NULL;

    if (argv[optind]) {
	input_file_name = xstrdup(argv[optind]);
	argv[optind] = normalize_quotes(argv[optind], "argument");
    }

    return input_file_name;
}

/* Reading the options.  */

/* Test whether getopt found an option ``A''.
   Assumes the option index is in the variable `option_index', and the
   option table in a variable `long_options'.  */
#define ARGUMENT_IS(a) STREQ (long_options[option_index].name, a)

/* SunOS cc can't initialize automatic structs, so make this static.  */
static struct option long_options[]
  = { { DUMP_OPTION,                 1, 0, 0 },
#ifdef TeX
      /* FIXME: Obsolete -- for backward compatibility only. */
      { "efmt",                      1, 0, 0 },
#endif
      { "help",                      0, 0, 0 },
      { "ini",                       0, &ini_version, 1 },
      { "interaction",               1, 0, 0 },
      { "halt-on-error",             0, &halt_on_error_p, 1 },
      { "progname",                  1, 0, 0 },
      { "version",                   0, 0, 0 },
#ifdef TeX
#if !defined(Aleph)
      { "mltex",                     0, &mltex_p, 1 },
#if !defined(XeTeX) && !IS_pTeX
      { "enc",                       0, &enctexp, 1 },
#endif
#endif /* !Aleph */
#if IS_eTeX
      { "etex",                      0, &etex_p, 1 },
#endif
      { "output-comment",            1, 0, 0 },
#if defined(pdfTeX)
      { "draftmode",                 0, 0, 0 },
      { "output-format",             1, 0, 0 },
#endif /* pdfTeX */
      { "shell-escape",              0, &shellenabledp, 1 },
      { "no-shell-escape",           0, &shellenabledp, -1 },
      { "enable-write18",            0, &shellenabledp, 1 },
      { "disable-write18",           0, &shellenabledp, -1 },
      { "shell-restricted",          0, 0, 0 },
      { "debug-format",              0, &debug_format_file, 1 },
      { "src-specials",              2, 0, 0 },
#if defined(__SyncTeX__)
      /* Synchronization: just like "interaction" above */
      { "synctex",                   1, 0, 0 },
#endif
#endif /* TeX */
#if defined (TeX) || defined (MF)
      { "file-line-error-style",     0, &file_line_error_style_p, 1 },
      { "no-file-line-error-style",  0, &file_line_error_style_p, -1 },
      /* Shorter option names for the above. */
      { "file-line-error",           0, &file_line_error_style_p, 1 },
      { "no-file-line-error",        0, &file_line_error_style_p, -1 },
      { "jobname",                   1, 0, 0 },
      { "parse-first-line",          0, &parse_first_line_p, 1 },
      { "no-parse-first-line",       0, &parse_first_line_p, -1 },
#if !defined(Aleph)
      { "translate-file",            1, 0, 0 },
      { "default-translate-file",    1, 0, 0 },
      { "8bit",                      0, &eight_bit_p, 1 },
#endif /* !Aleph */
#if defined(XeTeX)
      { "no-pdf",                    0, &no_pdf_output, 1 },
      { "output-driver",             1, 0, 0 },
      { "papersize",                 1, 0, 0 },
#endif /* XeTeX */
#endif /* TeX or MF */
#if IS_pTeX
#ifdef WIN32
      { "sjis-terminal",             0, &sjisterminal, 1 },
      { "guess-input-enc",           0, &infile_enc_auto, 1 },
      { "no-guess-input-enc",        0, &infile_enc_auto, 0 },
#endif
      { "kanji",                     1, 0, 0 },
      { "kanji-internal",            1, 0, 0 },
#endif /* IS_pTeX */
      { 0, 0, 0, 0 } };

static void
parse_options (int argc, string *argv)
{
  int g;   /* `getopt' return code.  */
  int option_index;

  for (;;) {
    g = getopt_long_only (argc, argv, "+", long_options, &option_index);

    if (g == -1) /* End of arguments, exit the loop.  */
      break;

    if (g == '?') { /* Unknown option.  */
      /* FIXME: usage (argv[0]); replaced by continue. */
      continue;
    }

    assert (g == 0); /* We have no short option names.  */

    if (ARGUMENT_IS ("progname")) {
      user_progname = optarg;

#ifdef XeTeX
    } else if (ARGUMENT_IS ("papersize")) {
      papersize = optarg;
    } else if (ARGUMENT_IS ("output-driver")) {
      outputdriver = optarg;
#endif

    } else if (ARGUMENT_IS ("jobname")) {
#ifdef XeTeX
      c_job_name = optarg;
#else
      c_job_name = normalize_quotes (optarg, "jobname");
#endif

    } else if (ARGUMENT_IS (DUMP_OPTION)) {
      dump_name = optarg;
      dump_option = true;

#ifdef TeX
    /* FIXME: Obsolete -- for backward compatibility only. */
    } else if (ARGUMENT_IS ("efmt")) {
      dump_name = optarg;
      dump_option = true;
#endif

#ifdef TeX
    } else if (ARGUMENT_IS ("output-comment")) {
      unsigned len = strlen (optarg);
      if (len < 256) {
        output_comment = optarg;
      } else {
        WARNING2 ("Comment truncated to 255 characters from %d. (%s)",
                  len, optarg);
        output_comment = xmalloc (256);
        strncpy (output_comment, optarg, 255);
        output_comment[255] = 0;
      }
    } else if (ARGUMENT_IS ("shell-restricted")) {
      shellenabledp = 1;
      restrictedshell = 1;

    } else if (ARGUMENT_IS ("src-specials")) {
       last_source_name = xstrdup("");
       /* Option `--src" without any value means `auto' mode. */
       if (optarg == NULL) {
         insert_src_special_every_par = true;
         insert_src_special_auto = true;
         src_specials_option = true;
         src_specials_p = true;
       } else {
          parse_src_specials_option(optarg);
       }
#endif /* TeX */
#if defined(pdfTeX)
    } else if (ARGUMENT_IS ("output-format")) {
       pdfoutputoption = 1;
       if (strcmp(optarg, "dvi") == 0) {
         pdfoutputvalue = 0;
       } else if (strcmp(optarg, "pdf") == 0) {
         pdfoutputvalue = 2;
       } else {
         WARNING1 ("Ignoring unknown value `%s' for --output-format", optarg);
         pdfoutputoption = 0;
       }
    } else if (ARGUMENT_IS ("draftmode")) {
      pdfdraftmodeoption = 1;
      pdfdraftmodevalue = 1;
#endif /* pdfTeX */
#if defined (TeX) || defined (MF)
#if !defined(Aleph)
    } else if (ARGUMENT_IS ("translate-file")) {
      translate_filename = optarg;
    } else if (ARGUMENT_IS ("default-translate-file")) {
      default_translate_filename = optarg;
#endif /* !Aleph */
#endif /* TeX or MF */
    } else if (ARGUMENT_IS ("interaction")) {
        /* These numbers match @d's in *.ch */
      if (STREQ (optarg, "batchmode")) {
        interaction_option = 0;
      } else if (STREQ (optarg, "nonstopmode")) {
        interaction_option = 1;
      } else if (STREQ (optarg, "scrollmode")) {
        interaction_option = 2;
      } else if (STREQ (optarg, "errorstopmode")) {
        interaction_option = 3;
      } else {
        WARNING1 ("Ignoring unknown argument `%s' to --interaction", optarg);
      }
#if IS_pTeX
    } else if (ARGUMENT_IS ("kanji")) {
      if (!set_enc_string (optarg, NULL)) {
        WARNING1 ("Ignoring unknown argument `%s' to --kanji", optarg);
      }
    } else if (ARGUMENT_IS ("kanji-internal")) {
      if (!set_enc_string (NULL, optarg)) {
        WARNING1 ("Ignoring unknown argument `%s' to --kanji-internal", optarg);
      }
#endif

    } else if (ARGUMENT_IS ("help")) {
        usagehelp (XETEXHELP, BUG_ADDRESS);

#if defined(__SyncTeX__)
    } else if (ARGUMENT_IS ("synctex")) {
		/* Synchronize TeXnology: catching the command line option as a long  */
		synctexoption = (int) strtol(optarg, NULL, 0);
#endif

    } else if (ARGUMENT_IS ("version")) {
        char *versions;
#if defined (pdfTeX) || defined(XeTeX)
        initversionstring(&versions);
#else
        versions = NULL;
#endif
        printversionandexit (BANNER, COPYRIGHT_HOLDER, AUTHOR, versions);

    } /* Else it was a flag; getopt has already done the assignment.  */
  }
}

#if defined(TeX)
void
parse_src_specials_option (const_string opt_list)
{
  char * toklist = xstrdup(opt_list);
  char * tok;
  insert_src_special_auto = false;
  tok = strtok (toklist, ", ");
  while (tok) {
    if (strcmp (tok, "everypar") == 0
        || strcmp (tok, "par") == 0
        || strcmp (tok, "auto") == 0) {
      insert_src_special_auto = true;
      insert_src_special_every_par = true;
    } else if (strcmp (tok, "everyparend") == 0
               || strcmp (tok, "parend") == 0)
      insert_src_special_every_parend = true;
    else if (strcmp (tok, "everycr") == 0
             || strcmp (tok, "cr") == 0)
      insert_src_special_every_cr = true;
    else if (strcmp (tok, "everymath") == 0
             || strcmp (tok, "math") == 0)
      insert_src_special_every_math = true;
    else if (strcmp (tok, "everyhbox") == 0
             || strcmp (tok, "hbox") == 0)
      insert_src_special_every_hbox = true;
    else if (strcmp (tok, "everyvbox") == 0
             || strcmp (tok, "vbox") == 0)
      insert_src_special_every_vbox = true;
    else if (strcmp (tok, "everydisplay") == 0
             || strcmp (tok, "display") == 0)
      insert_src_special_every_display = true;
    else if (strcmp (tok, "none") == 0) {
      /* This one allows to reset an option that could appear in texmf.cnf */
      insert_src_special_auto = insert_src_special_every_par =
        insert_src_special_every_parend = insert_src_special_every_cr =
        insert_src_special_every_math =  insert_src_special_every_hbox =
        insert_src_special_every_vbox = insert_src_special_every_display = false;
    } else {
      WARNING1 ("Ignoring unknown argument `%s' to --src-specials", tok);
    }
    tok = strtok(0, ", ");
  }
  free(toklist);
  src_specials_p=insert_src_special_auto | insert_src_special_every_par |
    insert_src_special_every_parend | insert_src_special_every_cr |
    insert_src_special_every_math |  insert_src_special_every_hbox |
    insert_src_special_every_vbox | insert_src_special_every_display;
  src_specials_option = true;
}
#endif

static void
parse_first_line (const_string filename)
{
}

/*
  piped I/O
 */

/* The code that implements popen() needs an array for tracking
   possible pipe file pointers, because these need to be
   closed using pclose().
*/

#if ENABLE_PIPES

#define NUM_PIPES 16

static FILE *pipes [NUM_PIPES];

boolean
open_in_or_pipe (FILE **f_ptr, int filefmt, const_string fopen_mode)
{
    string fname = NULL;
    int i; /* iterator */

    /* opening a read pipe is straightforward, only have to
       skip past the pipe symbol in the file name. filename
       quoting is assumed to happen elsewhere (it does :-)) */

    if (shellenabledp && *(name_of_file+1) == '|') {
      /* the user requested a pipe */
      *f_ptr = NULL;
      fname = xmalloc(strlen((const_string)(name_of_file+1))+1);
      strcpy(fname,(const_string)(name_of_file+1));
      if (fullnameoffile)
        free(fullnameoffile);
      fullnameoffile = xstrdup(fname);
      /*recorder_record_input (fname + 1);*/
      *f_ptr = runpopen(fname+1,"r");
      free(fname);
      for (i=0; i<NUM_PIPES; i++) {
        if (pipes[i]==NULL) {
          pipes[i] = *f_ptr;
          break;
        }
      }
      if (*f_ptr)
        setvbuf (*f_ptr,NULL,_IONBF,0);
#ifdef WIN32
      Poptr = *f_ptr;
#endif

      return *f_ptr != NULL;
    }

    return open_input(f_ptr,filefmt,fopen_mode) ;
}

#ifdef XeTeX
boolean
u_open_in_or_pipe(unicodefile* f, integer filefmt, const_string fopen_mode, integer mode, integer encodingData)
{
    string fname = NULL;
    int i; /* iterator */

    /* opening a read pipe is straightforward, only have to
       skip past the pipe symbol in the file name. filename
       quoting is assumed to happen elsewhere (it does :-)) */

    if (shellenabledp && *(name_of_file+1) == '|') {
      /* the user requested a pipe */
      *f = malloc(sizeof(UFILE));
      (*f)->encodingMode = (mode == AUTO) ? UTF8 : mode;
      (*f)->conversionData = 0;
      (*f)->savedChar = -1;
      (*f)->skipNextLF = 0;
      (*f)->f = NULL;
      fname = xmalloc(strlen((const_string)(name_of_file+1))+1);
      strcpy(fname,(const_string)(name_of_file+1));
      if (fullnameoffile)
        free(fullnameoffile);
      fullnameoffile = xstrdup(fname);
      /*recorder_record_input (fname + 1);*/
      (*f)->f = runpopen(fname+1,"r");
      free(fname);
      for (i=0; i<NUM_PIPES; i++) {
        if (pipes[i]==NULL) {
          pipes[i] = (*f)->f;
          break;
        }
      }
      if ((*f)->f)
        setvbuf ((*f)->f,NULL,_IONBF,0);
#ifdef WIN32
      Poptr = (*f)->f;
#endif

      return (*f)->f != NULL;
    }

    return real_u_open_in(f, filefmt, fopen_mode, mode, encodingData);
}
#endif

boolean
open_out_or_pipe (FILE **f_ptr, const_string fopen_mode)
{
    string fname;
    int i; /* iterator */

    /* opening a write pipe takes a little bit more work, because TeX
       will perhaps have appended ".tex".  To avoid user confusion as
       much as possible, this extension is stripped only when the command
       is a bare word.  Some small string trickery is needed to make
       sure the correct number of bytes is free()-d afterwards */

    if (shellenabledp && *(name_of_file+1) == '|') {
      /* the user requested a pipe */
      fname = xmalloc(strlen((const_string)(name_of_file+1))+1);
      strcpy(fname,(const_string)(name_of_file+1));
      if (strchr (fname,' ')==NULL && strchr(fname,'>')==NULL) {
        /* mp and mf currently do not use this code, but it
           is better to be prepared */
        if (STREQ((fname+strlen(fname)-4),".tex"))
          *(fname+strlen(fname)-4) = 0;
        *f_ptr = runpopen(fname+1,"w");
        *(fname+strlen(fname)) = '.';
      } else {
        *f_ptr = runpopen(fname+1,"w");
      }
      /*recorder_record_output (fname + 1);*/
      free(fname);

      for (i=0; i<NUM_PIPES; i++) {
        if (pipes[i]==NULL) {
          pipes[i] = *f_ptr;
          break;
        }
      }

      if (*f_ptr)
        setvbuf(*f_ptr,NULL,_IONBF,0);

      return *f_ptr != NULL;
    }

    return open_output(f_ptr,fopen_mode);
}


void
close_file_or_pipe (FILE *f)
{
  int i; /* iterator */

  if (shellenabledp) {
    /* if this file was a pipe, pclose() it and return */
    for (i=0; i<NUM_PIPES; i++) {
      if (pipes[i] == f) {
        if (f) {
          pclose (f);
#ifdef WIN32
          Poptr = NULL;
#endif
        }
        pipes[i] = NULL;
        return;
      }
    }
  }
  close_file(f);
}
#endif /* ENABLE_PIPES */

/* All our interrupt handler has to do is set TeX's or Metafont's global
   variable `interrupt'; then they will do everything needed.  */
#ifdef WIN32
/* Win32 doesn't set SIGINT ... */
static BOOL WINAPI
catch_interrupt (DWORD arg)
{
  switch (arg) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
    interrupt = 1;
    return TRUE;
  default:
    /* No need to set interrupt as we are exiting anyway */
    return FALSE;
  }
}
#else /* not WIN32 */
static RETSIGTYPE
catch_interrupt (int arg)
{
  interrupt = 1;
#ifdef OS2
  (void) signal (SIGINT, SIG_ACK);
#else
  (void) signal (SIGINT, catch_interrupt);
#endif /* not OS2 */
}
#endif /* not WIN32 */

#if defined(_MSC_VER)
#define strtoull _strtoui64
#endif

static boolean start_time_set = false;
static time_t start_time = 0;

void init_start_time() {
    char *source_date_epoch;
    unsigned long long epoch;
    char *endptr;
    if (!start_time_set) {
        start_time_set = true;
#ifndef onlyTeX
        source_date_epoch = getenv("SOURCE_DATE_EPOCH");
        if (source_date_epoch) {
            errno = 0;
            epoch = strtoull(source_date_epoch, &endptr, 10);
            if (epoch < 0 || *endptr != '\0' || errno != 0) {
FATAL1 ("invalid epoch-seconds-timezone value for environment variable $SOURCE_DATE_EPOCH: %s",
                      source_date_epoch);
            }
#if defined(_MSC_VER)
            if (epoch > 32535291599ULL)
                epoch = 32535291599ULL;
#endif
            start_time = epoch;
        } else
#endif /* not onlyTeX */
        {
            start_time = time((time_t *) NULL);
        }
    }
}

/* Besides getting the date and time here, we also set up the interrupt
   handler, for no particularly good reason.  It's just that since the
   `fix_date_and_time' routine is called early on (section 1337 in TeX,
   ``Get the first line of input and prepare to start''), this is as
   good a place as any.  */

void
get_date_and_time (integer *minutes,  integer *day,
                   integer *month,  integer *year)
{
  struct tm *tmptr;
#ifndef onlyTeX
  string sde_texprim = getenv ("FORCE_SOURCE_DATE");
  if (sde_texprim && STREQ (sde_texprim, "1")) {
    init_start_time ();
    tmptr = gmtime (&start_time);
  } else
#endif /* not onlyTeX */
    {
    /* whether the envvar was not set (usual case) or invalid,
       use current time.  */
    time_t myclock = time ((time_t *) 0);
    tmptr = localtime (&myclock);

#ifndef onlyTeX
    /* warn if they gave an invalid value, empty (null string) ok.  */
    if (sde_texprim && strlen (sde_texprim) > 0
        && !STREQ (sde_texprim, "0")) {
WARNING1 ("invalid value (expected 0 or 1) for environment variable $FORCE_SOURCE_DATE: %s",
          sde_texprim);
    }
#endif /* not onlyTeX */
  }

  *minutes = tmptr->tm_hour * 60 + tmptr->tm_min;
  *day = tmptr->tm_mday;
  *month = tmptr->tm_mon + 1;
  *year = tmptr->tm_year + 1900;

  {
#ifdef SA_INTERRUPT
    /* Under SunOS 4.1.x, the default action after return from the
       signal handler is to restart the I/O if nothing has been
       transferred.  The effect on TeX is that interrupts are ignored if
       we are waiting for input.  The following tells the system to
       return EINTR from read() in this case.  From ken@cs.toronto.edu.  */

    struct sigaction a, oa;

    a.sa_handler = catch_interrupt;
    sigemptyset (&a.sa_mask);
    sigaddset (&a.sa_mask, SIGINT);
    a.sa_flags = SA_INTERRUPT;
    sigaction (SIGINT, &a, &oa);
    if (oa.sa_handler != SIG_DFL)
      sigaction (SIGINT, &oa, (struct sigaction *) 0);
#else /* no SA_INTERRUPT */
#ifdef WIN32
    SetConsoleCtrlHandler(catch_interrupt, TRUE);
#else /* not WIN32 */
    RETSIGTYPE (*old_handler)(int);

    old_handler = signal (SIGINT, catch_interrupt);
    if (old_handler != SIG_DFL)
      signal (SIGINT, old_handler);
#endif /* not WIN32 */
#endif /* no SA_INTERRUPT */
  }
}

#if defined(pdfTeX) || defined(epTeX) || defined(eupTeX)
/*
 Getting a high resolution time.
 */
void
get_seconds_and_micros (integer *seconds,  integer *micros)
{
#if defined (HAVE_GETTIMEOFDAY)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *seconds = tv.tv_sec;
  *micros  = tv.tv_usec;
#elif defined (HAVE_FTIME)
  struct timeb tb;
  ftime(&tb);
  *seconds = tb.time;
  *micros  = tb.millitm*1000;
#else
  time_t myclock = time((time_t*)NULL);
  *seconds = myclock;
  *micros  = 0;
#endif
}
#endif

/* This string specifies what the `e' option does in response to an
   error message.  */
static const_string edit_value = EDITOR;

/* This procedure originally due to sjc@s1-c.  TeX & Metafont call it when
   the user types `e' in response to an error, invoking a text editor on
   the erroneous source file.  FNSTART is how far into FILENAME the
   actual filename starts; FNLENGTH is how long the filename is.  */

void
call_edit (packedASCIIcode *filename,
          pool_pointer fnstart,
          integer fnlength,
          integer linenumber)
{
  char *temp, *command, *fullcmd;
  char c;
  int sdone, ddone, i;

#ifdef WIN32
  char *fp, *ffp, *env, editorname[256], buffer[256];
  int cnt = 0;
  int dontchange = 0;
#endif

  sdone = ddone = 0;
  filename += fnstart;

  /* Close any open input files, since we're going to kill the job.  */
  for (i = 1; i <= in_open; i++)
#ifdef XeTeX
    xfclose (input_file[i]->f, "inputfile");
#else
    xfclose (input_file[i], "inputfile");
#endif

  /* Construct the command string.  The `11' is the maximum length an
     integer might be.  */
  command = xmalloc (strlen (edit_value) + fnlength + 11);

  /* So we can construct it as we go.  */
  temp = command;

#ifdef WIN32
  fp = editorname;
  if ((isalpha(*edit_value) && *(edit_value + 1) == ':'
        && IS_DIR_SEP (*(edit_value + 2)))
      || (*edit_value == '"' && isalpha(*(edit_value + 1))
        && *(edit_value + 2) == ':'
        && IS_DIR_SEP (*(edit_value + 3)))
     )
    dontchange = 1;
#endif

  while ((c = *edit_value++) != 0)
    {
      if (c == '%')
        {
          switch (c = *edit_value++)
            {
	    case 'd':
	      if (ddone)
                FATAL ("call_edit: `%%d' appears twice in editor command");
              sprintf (temp, "%ld", (long int)linenumber);
              while (*temp != '\0')
                temp++;
              ddone = 1;
              break;

	    case 's':
              if (sdone)
                FATAL ("call_edit: `%%s' appears twice in editor command");
              for (i =0; i < fnlength; i++)
		*temp++ = Xchr (filename[i]);
              sdone = 1;
              break;

	    case '\0':
              *temp++ = '%';
              /* Back up to the null to force termination.  */
	      edit_value--;
	      break;

	    default:
	      *temp++ = '%';
	      *temp++ = c;
	      break;
	    }
	}
      else {
#ifdef WIN32
        if (dontchange)
          *temp++ = c;
        else { if(Isspace(c) && cnt == 0) {
            cnt++;
            temp = command;
            *temp++ = c;
            *fp = '\0';
	  } else if(!Isspace(c) && cnt == 0) {
            *fp++ = c;
	  } else {
            *temp++ = c;
	  }
        }
#else
        *temp++ = c;
#endif
      }
    }

  *temp = 0;

#ifdef WIN32
  if (dontchange == 0) {
    if(editorname[0] == '.' ||
       editorname[0] == '/' ||
       editorname[0] == '\\') {
      fprintf(stderr, "%s is not allowed to execute.\n", editorname);
      uexit(1);
    }
    env = (char *)getenv("PATH");
    if(SearchPath(env, editorname, ".exe", 256, buffer, &ffp)==0) {
      if(SearchPath(env, editorname, ".bat", 256, buffer, &ffp)==0) {
        fprintf(stderr, "I cannot find %s in the PATH.\n", editorname);
        uexit(1);
      }
    }
    fullcmd = (char *)xmalloc(strlen(buffer)+strlen(command)+5);
    strcpy(fullcmd, "\"");
    strcat(fullcmd, buffer);
    strcat(fullcmd, "\"");
    strcat(fullcmd, command);
  } else
#endif
  fullcmd = command;

  /* Execute the command.  */
  if (system (fullcmd) != 0)
    fprintf (stderr, "! Trouble executing `%s'.\n", command);

  /* Quit, since we found an error.  */
  uexit (1);
}

/* Read and write dump files.  As distributed, these files are
   architecture dependent; specifically, BigEndian and LittleEndian
   architectures produce different files.  These routines always output
   BigEndian files.  This still does not guarantee them to be
   architecture-independent, because it is possible to make a format
   that dumps a glue ratio, i.e., a floating-point number.  Fortunately,
   none of the standard formats do that.  */

#if !defined (WORDS_BIGENDIAN) && !defined (NO_DUMP_SHARE) /* this fn */

/* This macro is always invoked as a statement.  It assumes a variable
   `temp'.  */

#define SWAP(x, y) temp = (x); (x) = (y); (y) = temp


/* Make the NITEMS items pointed at by P, each of size SIZE, be the
   opposite-endianness of whatever they are now.  */

static void
swap_items (char *p, int nitems, int size)
{
  char temp;

  /* Since `size' does not change, we can write a while loop for each
     case, and avoid testing `size' for each time.  */
  switch (size)
    {
    /* 16-byte items happen on the DEC Alpha machine when we are not
       doing sharable memory dumps.  */
    case 16:
      while (nitems--)
        {
          SWAP (p[0], p[15]);
          SWAP (p[1], p[14]);
          SWAP (p[2], p[13]);
          SWAP (p[3], p[12]);
          SWAP (p[4], p[11]);
          SWAP (p[5], p[10]);
          SWAP (p[6], p[9]);
          SWAP (p[7], p[8]);
          p += size;
        }
      break;

    case 8:
      while (nitems--)
        {
          SWAP (p[0], p[7]);
          SWAP (p[1], p[6]);
          SWAP (p[2], p[5]);
          SWAP (p[3], p[4]);
          p += size;
        }
      break;

    case 4:
      while (nitems--)
        {
          SWAP (p[0], p[3]);
          SWAP (p[1], p[2]);
          p += size;
        }
      break;

    case 2:
      while (nitems--)
        {
          SWAP (p[0], p[1]);
          p += size;
        }
      break;

    case 1:
      /* Nothing to do.  */
      break;

    default:
      FATAL1 ("Can't swap a %d-byte item for (un)dumping", size);
  }
}
#endif /* not WORDS_BIGENDIAN and not NO_DUMP_SHARE */


/* Here we write NITEMS items, each item being ITEM_SIZE bytes long.
   The pointer to the stuff to write is P, and we write to the file
   OUT_FILE.  */

void
#ifdef XeTeX
do_dump (char *p, int item_size, int nitems,  gzFile out_file)
#else
do_dump (char *p, int item_size, int nitems,  FILE *out_file)
#endif
{
#if !defined (WORDS_BIGENDIAN) && !defined (NO_DUMP_SHARE)
  swap_items (p, nitems, item_size);
#endif

#ifdef XeTeX
  if (gzwrite (out_file, p, item_size * nitems) != item_size * nitems)
#else
  if (fwrite (p, item_size, nitems, out_file) != nitems)
#endif
    {
      fprintf (stderr, "! Could not write %d %d-byte item(s) to %s.\n",
               nitems, item_size, name_of_file+1);
      uexit (1);
    }

  /* Have to restore the old contents of memory, since some of it might
     get used again.  */
#if !defined (WORDS_BIGENDIAN) && !defined (NO_DUMP_SHARE)
  swap_items (p, nitems, item_size);
#endif
}


/* Here is the dual of the writing routine.  */

void
#ifdef XeTeX
do_undump (char *p, int item_size, int nitems, gzFile in_file)
#else
do_undump (char *p, int item_size, int nitems, FILE *in_file)
#endif
{
#ifdef XeTeX
  if (gzread (in_file, p, item_size * nitems) != item_size * nitems)
#else
  if (fread (p, item_size, nitems, in_file) != (size_t) nitems)
#endif
    FATAL3 ("Could not undump %d %d-byte item(s) from %s",
            nitems, item_size, name_of_file+1);

#if !defined (WORDS_BIGENDIAN) && !defined (NO_DUMP_SHARE)
  swap_items (p, nitems, item_size);
#endif
}

/* FIXME -- some (most?) of this can/should be moved to the Pascal/WEB side. */
#if defined(TeX) || defined(MF)
#if !defined(pdfTeX)
static void
checkpool_pointer (pool_pointer pool_ptr, size_t len)
{
  if (pool_ptr + len >= pool_size) {
    fprintf (stderr, "\nstring pool overflow [%i bytes]\n",
            (int)pool_size); /* fixme */
    exit(1);
  }
}

#ifndef XeTeX	/* XeTeX uses this from XeTeX_ext.c */
static
#endif
int
maketexstring(const_string s)
{
  size_t len;
#ifdef XeTeX
  UInt32 rval;
  const unsigned char *cp = (const unsigned char *)s;
#endif
#if defined(TeX)
  if (s == NULL || *s == 0)
    return get_nullstr();
#else
  assert (s != 0);
#endif
  len = strlen(s);
  checkpool_pointer (pool_ptr, len); /* in the XeTeX case, this may be more than enough */
#ifdef XeTeX
  while ((rval = *(cp++)) != 0) {
    UInt16 extraBytes = bytesFromUTF8[rval];
    switch (extraBytes) { /* note: code falls through cases! */
      case 5: rval <<= 6; if (*cp) rval += *(cp++);
      case 4: rval <<= 6; if (*cp) rval += *(cp++);
      case 3: rval <<= 6; if (*cp) rval += *(cp++);
      case 2: rval <<= 6; if (*cp) rval += *(cp++);
      case 1: rval <<= 6; if (*cp) rval += *(cp++);
      case 0: ;
    };
    rval -= offsetsFromUTF8[extraBytes];
    if (rval > 0xffff) {
      rval -= 0x10000;
      str_pool[pool_ptr++] = 0xd800 + rval / 0x0400;
      str_pool[pool_ptr++] = 0xdc00 + rval % 0x0400;
    }
    else
      str_pool[pool_ptr++] = rval;
  }
#else /* ! XeTeX */
  while (len-- > 0)
    str_pool[pool_ptr++] = *s++;
#endif /* ! XeTeX */

  return make_string();
}
#endif /* !pdfTeX */

strnumber
make_full_name_string(void)
{
  return maketexstring(fullnameoffile);
}

/* Get the job name to be used, which may have been set from the
   command line. */
strnumber
get_job_name(strnumber name)
{
    strnumber ret = name;
    if (c_job_name != NULL)
      ret = maketexstring(c_job_name);
    return ret;
}
#endif

#if defined(TeX)
static int
compare_paths (const_string p1, const_string p2)
{
  int ret;
  while (
#ifdef MONOCASE_FILENAMES
                (((ret = (toupper(*p1) - toupper(*p2))) == 0) && (*p2 != 0))
#else
         (((ret = (*p1 - *p2)) == 0) && (*p2 != 0))
#endif
                || (IS_DIR_SEP(*p1) && IS_DIR_SEP(*p2))) {
       p1++, p2++;
  }
  ret = (ret < 0 ? -1 : (ret > 0 ? 1 : 0));
  return ret;
}

#ifdef XeTeX /* the string pool is UTF-16 but we want a UTF-8 string */

string
gettexstring (strnumber s)
{
  unsigned bytesToWrite = 0;
  pool_pointer len, i, j;
  string name;
  len = str_start[s + 1 - 65536L] - str_start[s - 65536L];
  name = xmalloc(len * 3 + 1); /* max UTF16->UTF8 expansion
                                  (code units, not bytes) */
  for (i = 0, j = 0; i < len; i++) {
    unsigned c = str_pool[i + str_start[s - 65536L]];
    if (c >= 0xD800 && c <= 0xDBFF) {
      unsigned lo = str_pool[++i + str_start[s - 65536L]];
      if (lo >= 0xDC00 && lo <= 0xDFFF)
        c = (c - 0xD800) * 0x0400 + lo - 0xDC00;
      else
        c = 0xFFFD;
    }
    if (c < 0x80)
      bytesToWrite = 1;
    else if (c < 0x800)
      bytesToWrite = 2;
    else if (c < 0x10000)
      bytesToWrite = 3;
    else if (c < 0x110000)
      bytesToWrite = 4;
    else {
      bytesToWrite = 3;
      c = 0xFFFD;
    }

    j += bytesToWrite;
    switch (bytesToWrite) { /* note: everything falls through. */
      case 4: name[--j] = ((c | 0x80) & 0xBF); c >>= 6;
      case 3: name[--j] = ((c | 0x80) & 0xBF); c >>= 6;
      case 2: name[--j] = ((c | 0x80) & 0xBF); c >>= 6;
      case 1: name[--j] =  (c | firstByteMark[bytesToWrite]);
    }
    j += bytesToWrite;
  }
  name[j] = 0;
  return name;
}

#else

string
gettexstring (strnumber s)
{
  pool_pointer len;
  string name;
#if !defined(Aleph)
  len = str_start[s + 1] - str_start[s];
#else
  len = str_startar[s + 1 - 65536L] - str_startar[s - 65536L];
#endif
  name = (string)xmalloc (len + 1);
#if !defined(Aleph)
  strncpy (name, (string)&str_pool[str_start[s]], len);
#else
  {
  pool_pointer i;
  /* Don't use strncpy.  The str_pool is not made up of chars. */
  for (i=0; i<len; i++) name[i] =  str_pool[i+str_startar[s - 65536L]];
  }
#endif
  name[len] = 0;
  return name;
}

#endif /* not XeTeX */

boolean
is_new_source (strnumber srcfilename, int lineno)
{
  char *name = gettexstring(srcfilename);
  return (compare_paths(name, last_source_name) != 0 || lineno != last_lineno);
}

void
remember_source_info (strnumber srcfilename, int lineno)
{
  if (last_source_name)
       free(last_source_name);
  last_source_name = gettexstring(srcfilename);
  last_lineno = lineno;
}

pool_pointer
make_src_special (strnumber srcfilename, int lineno)
{
  pool_pointer oldpool_ptr = pool_ptr;
  char *filename = gettexstring(srcfilename);
  /* FIXME: Magic number. */
  char buf[40];
  char *s = buf;

  /* Always put a space after the number, which makes things easier
   * to parse.
   */
  sprintf (buf, "src:%d ", lineno);

  if (pool_ptr + strlen(buf) + strlen(filename) >= (size_t)pool_size) {
       fprintf (stderr, "\nstring pool overflow\n"); /* fixme */
       exit (1);
  }
  s = buf;
  while (*s)
    str_pool[pool_ptr++] = *s++;

  s = filename;
  while (*s)
    str_pool[pool_ptr++] = *s++;

  return (oldpool_ptr);
}

/* pdfTeX routines also used for e-pTeX, e-upTeX, and XeTeX */
#if defined (pdfTeX) || defined (epTeX) || defined (eupTeX) || defined(XeTeX)

#include <sys/stat.h>
#include "md5.h"

#define check_nprintf(size_get, size_want) \
    if ((unsigned)(size_get) >= (unsigned)(size_want)) \
        pdftex_fail ("snprintf failed: file %s, line %d", __FILE__, __LINE__);
#  define check_buf(size, buf_size)                         \
    if ((unsigned)(size) > (unsigned)(buf_size))            \
        pdftex_fail("buffer overflow at file %s, line %d", __FILE__,  __LINE__ )
#  define xfree(p)            do { if (p != NULL) free(p); p = NULL; } while (0)
#  define MAX_CSTRING_LEN     1024 * 1024

#if !defined (pdfTeX)
#  define PRINTF_BUF_SIZE     1024
static char print_buf[PRINTF_BUF_SIZE];

/* Helper for pdftex_fail. */
static void safe_print(const char *str)
{
    const char *c;
    for (c = str; *c; ++c)
        print(*c);
}
/* pdftex_fail may be called when a buffer overflow has happened/is
   happening, therefore may not call mktexstring.  However, with the
   current implementation it appears that error messages are misleading,
   possibly because pool overflows are detected too late.

   The output format of this fuction must be the same as pdf_error in
   pdftex.web! */
__attribute__ ((noreturn, format(printf, 1, 2)))
void pdftex_fail(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    print_ln();
    safe_print("!error: ");
    vsnprintf(print_buf, PRINTF_BUF_SIZE, fmt, args);
    safe_print(print_buf);
    va_end(args);
    print_ln();
    safe_print(" ==> Fatal error occurred, output file will be damaged!");
    print_ln();
    exit(EXIT_FAILURE);
}
#endif /* not pdfTeX */

#if !defined(XeTeX)

#define TIME_STR_SIZE 30
char start_time_str[TIME_STR_SIZE];
static char time_str[TIME_STR_SIZE];
    /* minimum size for time_str is 24: "D:YYYYmmddHHMMSS+HH'MM'" */

static void makepdftime(time_t t, char *time_str, boolean utc)
{

    struct tm lt, gmt;
    size_t size;
    int i, off, off_hours, off_mins;

    /* get the time */
    if (utc) {
        lt = *gmtime(&t);
    }
    else {
        lt = *localtime(&t);
    }
    size = strftime(time_str, TIME_STR_SIZE, "D:%Y%m%d%H%M%S", &lt);
    /* expected format: "YYYYmmddHHMMSS" */
    if (size == 0) {
        /* unexpected, contents of time_str is undefined */
        time_str[0] = '\0';
        return;
    }

    /* correction for seconds: %S can be in range 00..61,
       the PDF reference expects 00..59,
       therefore we map "60" and "61" to "59" */
    if (time_str[14] == '6') {
        time_str[14] = '5';
        time_str[15] = '9';
        time_str[16] = '\0';    /* for safety */
    }

    /* get the time zone offset */
    gmt = *gmtime(&t);

    /* this calculation method was found in exim's tod.c */
    off = 60 * (lt.tm_hour - gmt.tm_hour) + lt.tm_min - gmt.tm_min;
    if (lt.tm_year != gmt.tm_year) {
        off += (lt.tm_year > gmt.tm_year) ? 1440 : -1440;
    } else if (lt.tm_yday != gmt.tm_yday) {
        off += (lt.tm_yday > gmt.tm_yday) ? 1440 : -1440;
    }

    if (off == 0) {
        time_str[size++] = 'Z';
        time_str[size] = 0;
    } else {
        off_hours = off / 60;
        off_mins = abs(off - off_hours * 60);
        i = snprintf(&time_str[size], 9, "%+03d'%02d'", off_hours, off_mins);
        check_nprintf(i, 9);
    }
}

void initstarttime(void)
{
    if (!start_time_set) {
        init_start_time ();
        if (getenv ("SOURCE_DATE_EPOCH")) {
            makepdftime (start_time, start_time_str, /* utc= */true);
        } else {
            makepdftime (start_time, start_time_str,  /* utc= */false);
        }
    }
}

char *makecstring(integer s)
{
    static char *cstrbuf = NULL;
    char *p;
    static int allocsize;
    int allocgrow, i, l = str_start[s + 1] - str_start[s];
    check_buf(l + 1, MAX_CSTRING_LEN);
    if (cstrbuf == NULL) {
        allocsize = l + 1;
        cstrbuf = xmalloc_array(char, allocsize);
    } else if (l + 1 > allocsize) {
        allocgrow = allocsize * 0.2;
        if (l + 1 - allocgrow > allocsize)
            allocsize = l + 1;
        else if (allocsize < MAX_CSTRING_LEN - allocgrow)
            allocsize += allocgrow;
        else
            allocsize = MAX_CSTRING_LEN;
        cstrbuf = xrealloc_array(cstrbuf, char, allocsize);
    }
    p = cstrbuf;
    for (i = 0; i < l; i++)
        *p++ = str_pool[i + str_start[s]];
    *p = 0;
    return cstrbuf;
}

/* makecfilename
  input/ouput same as makecstring:
    input: string number
    output: C string with quotes removed.
    That means, file names that are legal on some operation systems
    cannot any more be used since pdfTeX version 1.30.4.
*/
char *makecfilename(integer s)
{
    char *name = makecstring(s);
    char *p = name;
    char *q = name;

    while (*p) {
        if (*p != '"')
            *q++ = *p;
        p++;
    }
    *q = '\0';
    return name;
}

void getcreationdate(void)
{
    size_t len;
    initstarttime();
    /* put creation date on top of string pool and update pool_ptr */
    len = strlen(start_time_str);

    /* In e-pTeX, "init len => call initstarttime()" (as pdftexdir/utils.c)
       yields  unintentional output. */

    if ((unsigned) (pool_ptr + len) >= (unsigned) (pool_size)) {
        pool_ptr = pool_size;
        /* error by str_toks that calls str_room(1) */
        return;
    }

    memcpy(&str_pool[pool_ptr], start_time_str, len);
    pool_ptr += len;
}

void getfilemoddate(integer s)
{
    struct stat file_data;

    char *file_name = kpse_find_file(makecfilename(s), kpse_tex_format, true);
    if (file_name == NULL) {
        return;                 /* empty string */
    }

    /*recorder_record_input(file_name);*/
    /* get file status */
    if (stat(file_name, &file_data) == 0) {
        size_t len;

        makepdftime(file_data.st_mtime, time_str, /* utc= */false);
        len = strlen(time_str);
        if ((unsigned) (pool_ptr + len) >= (unsigned) (pool_size)) {
            pool_ptr = pool_size;
            /* error by str_toks that calls str_room(1) */
        } else {
            memcpy(&str_pool[pool_ptr], time_str, len);
            pool_ptr += len;
        }
    }
    /* else { errno contains error code } */

    xfree(file_name);
}

void getfilesize(integer s)
{
    struct stat file_data;
    int i;

    char *file_name = kpse_find_file(makecfilename(s), kpse_tex_format, true);
    if (file_name == NULL) {
        return;                 /* empty string */
    }

    /*recorder_record_input(file_name);*/
    /* get file status */
    if (stat(file_name, &file_data) == 0) {
        size_t len;
        char buf[20];

        /* st_size has type off_t */
        i = snprintf(buf, sizeof(buf),
                     "%lu", (long unsigned int) file_data.st_size);
        check_nprintf(i, sizeof(buf));
        len = strlen(buf);
        if ((unsigned) (pool_ptr + len) >= (unsigned) (pool_size)) {
            pool_ptr = pool_size;
            /* error by str_toks that calls str_room(1) */
        } else {
            memcpy(&str_pool[pool_ptr], buf, len);
            pool_ptr += len;
        }
    }
    /* else { errno contains error code } */

    xfree(file_name);
}

void getfiledump(integer s, int offset, int length)
{
    FILE *f;
    int read, i;
    pool_pointer data_ptr;
    pool_pointer data_end;
    char *file_name;

    if (length == 0) {
        /* empty result string */
        return;
    }

    if (pool_ptr + 2 * length + 1 >= pool_size) {
        /* no place for result */
        pool_ptr = pool_size;
        /* error by str_toks that calls str_room(1) */
        return;
    }

    file_name = kpse_find_file(makecfilename(s), kpse_tex_format, true);
    if (file_name == NULL) {
        return;                 /* empty string */
    }

    /* read file data */
    f = fopen(file_name, FOPEN_RBIN_MODE);
    if (f == NULL) {
        xfree(file_name);
        return;
    }
    /*recorder_record_input(file_name);*/
    if (fseek(f, offset, SEEK_SET) != 0) {
        xfree(file_name);
        return;
    }
    /* there is enough space in the string pool, the read
       data are put in the upper half of the result, thus
       the conversion to hex can be done without overwriting
       unconverted bytes. */
    data_ptr = pool_ptr + length;
    read = fread(&str_pool[data_ptr], sizeof(char), length, f);
    fclose(f);

    /* convert to hex */
    data_end = data_ptr + read;
    for (; data_ptr < data_end; data_ptr++) {
        i = snprintf((char *) &str_pool[pool_ptr], 3,
                     "%.2X", (unsigned int) str_pool[data_ptr]);
        check_nprintf(i, 3);
        pool_ptr += i;
    }
    xfree(file_name);
}
#endif /* not XeTeX */

/* Converts any given string in into an allowed PDF string which is
 * hexadecimal encoded;
 * sizeof(out) should be at least lin*2+1.
 */
void convertStringToHexString(const char *in, char *out, int lin)
{
    int i, j, k;
    char buf[3];
    j = 0;
    for (i = 0; i < lin; i++) {
        k = snprintf(buf, sizeof(buf),
                     "%02X", (unsigned int) (unsigned char) in[i]);
        check_nprintf(k, sizeof(buf));
        out[j++] = buf[0];
        out[j++] = buf[1];
    }
    out[j] = '\0';
}

#define DIGEST_SIZE 16
#define FILE_BUF_SIZE 1024

void getmd5sum(strnumber s, boolean file)
{
    md5_state_t state;
    md5_byte_t digest[DIGEST_SIZE];
    char outbuf[2 * DIGEST_SIZE + 1];
    int len = 2 * DIGEST_SIZE;
#if defined(XeTeX)
    char *xname;
    int i;
#endif

    if (file) {
        char file_buf[FILE_BUF_SIZE];
        int read = 0;
        FILE *f;
        char *file_name;

        xname = gettexstring (s);
        file_name = kpse_find_file (xname, kpse_tex_format, true);
        xfree (xname);
        if (file_name == NULL) {
            return;             /* empty string */
        }
        /* in case of error the empty string is returned,
           no need for xfopen that aborts on error.
         */
        f = fopen(file_name, FOPEN_RBIN_MODE);
        if (f == NULL) {
            xfree(file_name);
            return;
        }
        /*recorder_record_input(file_name);*/
        md5_init(&state);
        while ((read = fread(&file_buf, sizeof(char), FILE_BUF_SIZE, f)) > 0) {
            md5_append(&state, (const md5_byte_t *) file_buf, read);
        }
        md5_finish(&state, digest);
        fclose(f);

        xfree(file_name);
    } else {
        /* s contains the data */
        md5_init(&state);
#if defined(XeTeX)
        xname = gettexstring (s);
        md5_append(&state,
                   (md5_byte_t *) xname,
                   strlen (xname));
        xfree (xname);
#else
        md5_append(&state,
                   (md5_byte_t *) &str_pool[str_start[s]],
                   str_start[s + 1] - str_start[s]);
#endif
        md5_finish(&state, digest);
    }

    if (pool_ptr + len >= pool_size) {
        /* error by str_toks that calls str_room(1) */
        return;
    }
    convertStringToHexString((char *) digest, outbuf, DIGEST_SIZE);
#if defined(XeTeX)
    for (i = 0; i < 2 * DIGEST_SIZE; i++)
        str_pool[pool_ptr++] = (uint16_t)outbuf[i];
#else
    memcpy(&str_pool[pool_ptr], outbuf, len);
    pool_ptr += len;
#endif
}

#endif /* pdfTeX or e-pTeX or e-upTeX or XeTeX */
#endif /* TeX */

/* Metafont/MetaPost fraction routines. Replaced either by assembler or C.
   The assembler syntax doesn't work on Solaris/x86.  */
#ifndef TeX
#if defined (__sun__) || defined (__cplusplus)
#define NO_MF_ASM
#endif
/* The assembler code is not PIC safe on i?86 so use C code.  */
#if defined (__PIC__) && defined (__i386__)
#define NO_MF_ASM
#endif
#if defined(WIN32) && !defined(NO_MF_ASM) && !defined(__MINGW32__)
#include "lib/mfmpw32.c"
#elif defined (__i386__) && defined (__GNUC__) && !defined (NO_MF_ASM)
#include "lib/mfmpi386.asm"
#else
/* Replace fixed-point fraction routines from mf.web and mp.web with
   Hobby's floating-point C code.  */

/****************************************************************
Copyright 1990 - 1995 by AT&T Bell Laboratories.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
any of its entities not be used in advertising or publicity
pertaining to distribution of the software without specific,
written prior permission.

AT&T disclaims all warranties with regard to this software,
including all implied warranties of merchantability and fitness.
In no event shall AT&T be liable for any special, indirect or
consequential damages or any damages whatsoever resulting from
loss of use, data or profits, whether in an action of contract,
negligence or other tortious action, arising out of or in
connection with the use or performance of this software.
****************************************************************/

/**********************************************************
 The following is by John Hobby
 **********************************************************/

#ifndef FIXPT

/* These replacements for takefraction, makefraction, takescaled, makescaled
   run about 3 to 11 times faster than the standard versions on modern machines
   that have fast hardware for double-precision floating point.  They should
   produce approximately correct results on all machines and agree exactly
   with the standard versions on machines that satisfy the following conditions:
   1. Doubles must have at least 46 mantissa bits; i.e., numbers expressible
      as n*2^k with abs(n)<2^46 should be representable.
   2. The following should hold for addition, subtraction, and multiplcation but
      not necessarily for division:
      A. If the true answer is between two representable numbers, the computed
         answer must be one of them.
      B. When the true answer is representable, this must be the computed result.
   3. Dividing one double by another should always produce a relative error of
      at most one part in 2^46.  (This is why the mantissa requirement is
      46 bits instead of 45 bits.)
   3. In the absence of overflow, double-to-integer conversion should truncate
      toward zero and do this in an exact fashion.
   4. Integer-to-double convesion should produce exact results.
   5. Dividing one power of two by another should yield an exact result.
   6. ASCII to double conversion should be exact for integer values.
   7. Integer arithmetic must be done in the two's-complement system.
*/
#define ELGORDO  0x7fffffff
#define TWEXP31  2147483648.0
#define TWEXP28  268435456.0
#define TWEXP16 65536.0
#define TWEXP_16 (1.0/65536.0)
#define TWEXP_28 (1.0/268435456.0)

integer
ztakefraction (integer p, integer q)     /* Approximate p*q/2^28 */
{	register double d;
	register integer i;
	d = (double)p * (double)q * TWEXP_28;
	if ((p^q) >= 0) {
		d += 0.5;
		if (d>=TWEXP31) {
			if (d!=TWEXP31 || (((p&077777)*(q&077777))&040000)==0)
				aritherror = true;
			return ELGORDO;
		}
		i = (integer) d;
		if (d==i && (((p&077777)*(q&077777))&040000)!=0) --i;
	} else {
		d -= 0.5;
		if (d<= -TWEXP31) {
			if (d!= -TWEXP31 || ((-(p&077777)*(q&077777))&040000)==0)
				aritherror = true;
			return -ELGORDO;
		}
		i = (integer) d;
		if (d==i && ((-(p&077777)*(q&077777))&040000)!=0) ++i;
	}
	return i;
}

integer
ztakescaled (integer p, integer q)		/* Approximate p*q/2^16 */
{	register double d;
	register integer i;
	d = (double)p * (double)q * TWEXP_16;
	if ((p^q) >= 0) {
		d += 0.5;
		if (d>=TWEXP31) {
			if (d!=TWEXP31 || (((p&077777)*(q&077777))&040000)==0)
				aritherror = true;
			return ELGORDO;
		}
		i = (integer) d;
		if (d==i && (((p&077777)*(q&077777))&040000)!=0) --i;
	} else {
		d -= 0.5;
		if (d<= -TWEXP31) {
			if (d!= -TWEXP31 || ((-(p&077777)*(q&077777))&040000)==0)
				aritherror = true;
			return -ELGORDO;
		}
		i = (integer) d;
		if (d==i && ((-(p&077777)*(q&077777))&040000)!=0) ++i;
	}
	return i;
}

/* Note that d cannot exactly equal TWEXP31 when the overflow test is made
   because the exact value of p/q cannot be strictly between (2^31-1)/2^28
   and 8/1.  No pair of integers less than 2^31 has such a ratio.
*/
integer
zmakefraction (integer p, integer q)	/* Approximate 2^28*p/q */
{	register double d;
	register integer i;
#ifdef DEBUG
	if (q==0) confusion(47);
#endif /* DEBUG */
	d = TWEXP28 * (double)p /(double)q;
	if ((p^q) >= 0) {
		d += 0.5;
		if (d>=TWEXP31) {aritherror=true; return ELGORDO;}
		i = (integer) d;
		if (d==i && ( ((q>0 ? -q : q)&077777)
				* (((i&037777)<<1)-1) & 04000)!=0) --i;
	} else {
		d -= 0.5;
		if (d<= -TWEXP31) {aritherror=true; return -ELGORDO;}
		i = (integer) d;
		if (d==i && ( ((q>0 ? q : -q)&077777)
				* (((i&037777)<<1)+1) & 04000)!=0) ++i;
	}
	return i;
}

/* Note that d cannot exactly equal TWEXP31 when the overflow test is made
   because the exact value of p/q cannot be strictly between (2^31-1)/2^16
   and 2^15/1.  No pair of integers less than 2^31 has such a ratio.
*/
integer
zmakescaled (integer p, integer q)		/* Approximate 2^16*p/q */
{	register double d;
	register integer i;
#ifdef DEBUG
	if (q==0) confusion(47);
#endif /* DEBUG */
	d = TWEXP16 * (double)p /(double)q;
	if ((p^q) >= 0) {
		d += 0.5;
		if (d>=TWEXP31) {aritherror=true; return ELGORDO;}
		i = (integer) d;
		if (d==i && ( ((q>0 ? -q : q)&077777)
				* (((i&037777)<<1)-1) & 04000)!=0) --i;
	} else {
		d -= 0.5;
		if (d<= -TWEXP31) {aritherror=true; return -ELGORDO;}
		i = (integer) d;
		if (d==i && ( ((q>0 ? q : -q)&077777)
				* (((i&037777)<<1)+1) & 04000)!=0) ++i;
	}
	return i;
}

#endif /* not FIXPT */
#endif /* not assembler */
#endif /* not TeX, i.e., MF */

#ifdef MF
/* On-line display routines for Metafont.  Here we use a dispatch table
   indexed by the MFTERM or TERM environment variable to select the
   graphics routines appropriate to the user's terminal.  stdout must be
   connected to a terminal for us to do any graphics.  */

#ifdef MFNOWIN
#undef AMIGAWIN
#undef EPSFWIN
#undef HP2627WIN
#undef MFTALKWIN
#undef NEXTWIN
#undef REGISWIN
#undef SUNWIN
#undef TEKTRONIXWIN
#undef UNITERMWIN
#undef WIN32WIN
#undef X11WIN
#endif

/* Prototypes for Metafont display routines: mf_XXX_initscreen,
   mf_XXX_updatescreen, mf_XXX_blankrectangle, and mf_XXX_paintrow.  */
#include <window/mfdisplay.h>

/* This variable, `mfwsw', contains the dispatch tables for each
   terminal.  We map the Pascal calls to the routines `init_screen',
   `update_screen', `blank_rectangle', and `paint_row' into the
   appropriate entry point for the specific terminal that MF is being
   run on.  */

struct mfwin_sw
{
  const char *mfwsw_type;	/* Name of terminal a la TERMCAP.  */
  int (*mfwsw_initscreen) (void);
  void (*mfwsw_updatescrn) (void);
  void (*mfwsw_blankrect) (screencol, screencol, screenrow, screenrow);
  void (*mfwsw_paintrow) (screenrow, pixelcolor, transspec, screencol);
} mfwsw[] =
{
#ifdef AMIGAWIN
  { "amiterm", mf_amiga_initscreen, mf_amiga_updatescreen,
    mf_amiga_blankrectangle, mf_amiga_paintrow },
#endif
#ifdef EPSFWIN
  { "epsf", mf_epsf_initscreen, mf_epsf_updatescreen,
    mf_epsf_blankrectangle, mf_epsf_paintrow },
#endif
#ifdef HP2627WIN
  { "hp2627", mf_hp2627_initscreen, mf_hp2627_updatescreen,
    mf_hp2627_blankrectangle, mf_hp2627_paintrow },
#endif
#ifdef MFTALKWIN
  { "mftalk", mf_mftalk_initscreen, mf_mftalk_updatescreen,
     mf_mftalk_blankrectangle, mf_mftalk_paintrow },
#endif
#ifdef NEXTWIN
  { "next", mf_next_initscreen, mf_next_updatescreen,
    mf_next_blankrectangle, mf_next_paintrow },
#endif
#ifdef REGISWIN
  { "regis", mf_regis_initscreen, mf_regis_updatescreen,
    mf_regis_blankrectangle, mf_regis_paintrow },
#endif
#ifdef SUNWIN
  { "sun", mf_sun_initscreen, mf_sun_updatescreen,
    mf_sun_blankrectangle, mf_sun_paintrow },
#endif
#ifdef TEKTRONIXWIN
  { "tek", mf_tektronix_initscreen, mf_tektronix_updatescreen,
    mf_tektronix_blankrectangle, mf_tektronix_paintrow },
#endif
#ifdef UNITERMWIN
   { "uniterm", mf_uniterm_initscreen, mf_uniterm_updatescreen,
     mf_uniterm_blankrectangle, mf_uniterm_paintrow },
#endif
#ifdef WIN32WIN
  { "win32term", mf_win32_initscreen, mf_win32_updatescreen,
    mf_win32_blankrectangle, mf_win32_paintrow },
#endif
#ifdef X11WIN
  { "xterm", mf_x11_initscreen, mf_x11_updatescreen,
    mf_x11_blankrectangle, mf_x11_paintrow },
#endif

  /* Always support this.  */
  { "trap", mf_trap_initscreen, mf_trap_updatescreen,
    mf_trap_blankrectangle, mf_trap_paintrow },

/* Finally, we must have an entry with a terminal type of NULL.  */
  { NULL, NULL, NULL, NULL, NULL }

}; /* End of the array initialization.  */


/* This is a pointer to the mfwsw[] entry that we find.  */
static struct mfwin_sw *mfwp;


/* The following are routines that just jump to the correct
   terminal-specific graphics code. If none of the routines in the
   dispatch table exist, or they fail, we produce trap-compatible
   output, i.e., the same words and punctuation that the unchanged
   mf.web would produce.  */


/* This returns true if we can do window operations, else false.  */

boolean
initscreen (void)
{
  int retval;
  /* If MFTERM is set, use it.  */
  const_string tty_type = NULL;

  if (tty_type == NULL)
    {
#if defined (AMIGA)
      tty_type = "amiterm";
#elif defined (WIN32)
      tty_type = "win32term";
#elif defined (OS2) || defined (__DJGPP__) /* not AMIGA nor WIN32 */
      tty_type = "mftalk";
#else /* not (OS2 or WIN32 or __DJGPP__ or AMIGA) */
      /* If DISPLAY is set, we are X11; otherwise, who knows.  */
      boolean have_display = getenv ("DISPLAY") != NULL;
      tty_type = have_display ? "xterm" : getenv ("TERM");

      /* If we don't know what kind of terminal this is, or if Metafont
         isn't being run interactively, don't do any online output.  */
      if (tty_type == NULL
          || (!STREQ (tty_type, "trap") && !isatty (fileno (stdout))))
        return 0;
#endif /* not (OS2 or WIN32 or __DJGPP__ or AMIGA) */
  }

  /* Test each of the terminals given in `mfwsw' against the terminal
     type, and take the first one that matches, or if the user is running
     under Emacs, the first one.  */
  for (mfwp = mfwsw; mfwp->mfwsw_type != NULL; mfwp++) {
    if (!strncmp (mfwp->mfwsw_type, tty_type, strlen (mfwp->mfwsw_type))
	|| STREQ (tty_type, "emacs")) {
      if (mfwp->mfwsw_initscreen) {
	retval = (*mfwp->mfwsw_initscreen) ();
#ifdef WIN32
	Sleep(1000); /* Wait for opening a window */
#endif
	return retval;
      }
      else {
        fprintf (stderr, "mf: Couldn't initialize online display for `%s'.\n",
                 tty_type);
        break;
      }
    }
  }

  /* The current terminal type wasn't found in any of the entries, or
     initalization failed, so silently give up, assuming that the user
     isn't on a terminal that supports graphic output.  */
  return 0;
}


/* Make sure everything is visible.  */

void
updatescreen (void)
{
  if (mfwp->mfwsw_updatescrn)
    (*mfwp->mfwsw_updatescrn) ();
}


/* This sets the rectangle bounded by ([left,right], [top,bottom]) to
   the background color.  */

void
blankrectangle (screencol left, screencol right,
                screenrow top, screenrow bottom)
{
  if (mfwp->mfwsw_blankrect)
    (*mfwp->mfwsw_blankrect) (left, right, top, bottom);
}


/* This paints ROW, starting with the color INIT_COLOR.
   TRANSITION_VECTOR then specifies the length of the run; then we
   switch colors.  This goes on for VECTOR_SIZE transitions.  */

void
paintrow (screenrow row, pixelcolor init_color,
          transspec transition_vector, screencol vector_size)
{
  if (mfwp->mfwsw_paintrow)
    (*mfwp->mfwsw_paintrow) (row, init_color, transition_vector, vector_size);
}
#endif /* MF */