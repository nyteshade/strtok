# strtok
A command line tool that allows string splitting and processing.

## Overview

The purpose of this utility was originally for use with the Amiga line of personal computers and, more specifically, to allow for some custom processing within the Startup-Sequence and Shell-Startup and User-Startup scripts on a given boot disk. It, by default, will take a string in and split wherever there are spaces in the string. It will
give you the first non-space grouping of characters. Much like a call to `strtok("string", "token")` in C. 

It also allows the reading of a file's contents as a string when the FILE flag is specified. By default the token is a white space character but it can be set to a tab character or newline by passing in the appropriate flags. 

## Usage

```
Usage: StrTok <string> <desired-index> <token> [KEEP] [HELP] [FILE]
  <string>         the input string to work on
  <desired-index>  a 0 based index indicating which part to keep
  <token>          the string to split the input on (default: " ")

Flags:
  [HELP, -h, ?]    HELP or -h or ? or -help will invoke this output
  [KEEP]           when supplied, the tokens are appended to the preceeding
                   index. So "Hello,World" splitting on "," would show
                   index 0 as "Hello," and index 1 as "World"

  [FILE]           when the FILE mode is enabled, the <string> parameter is
                   considered to be a path to a file to read. The default
                   token used to split values is converted to a newline
                   character ("\n")
  
Hidden Flags:
  [DEBUG]          display the arguments as they were parsed
  [NEWLINE]        set the token to a newline chracter
  [TAB]            set the token to a tab character
  [SPACE]          set the token to a space character
```
