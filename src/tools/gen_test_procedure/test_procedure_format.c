#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

int max_issue=0;

// ======== ======== ======== ========
void parse_string(char *in,char *out)
{
  int outlen=0;
  for(int i=0;in[i];i++) {
    if (in[i]=='\\') {
      i++;
      switch(in[i]) {
      case 'r':	out[outlen++]='\r'; break;
      case 'n':	out[outlen++]='\r'; break;
      case 't':	out[outlen++]='\t'; break;
      case '\"':	out[outlen++]='\"'; break;
      case '\'':	out[outlen++]='\''; break;
      default:
        fprintf(stderr,"WARNING: Unknown \\ escape \\%c\n",in[i]);
        i--;
        out[outlen++]='\\';
      } // switch
    //
    } else {
      out[outlen++]=in[i];
    } // if
  //
  } // for

  out[outlen]=0;
}

// ======== ======== ======== ========
#define MAX_PROBLEMS 8192
int problem_issues[MAX_PROBLEMS];
char *problem_titles[MAX_PROBLEMS];
char *problem_descriptions[MAX_PROBLEMS];
int problem_mentioned[MAX_PROBLEMS];
int problem_count=0;

char tex_esc_buff[8192];

// ======== ======== ======== ========
char *tex_escape(char *in)
{
  int ol=0;
  for(;*in;in++) {
    switch(*in) {
    case '$':
    case '%':
      tex_esc_buff[ol++]='\\';
      tex_esc_buff[ol++]=*in;      
      break;
    default:
      tex_esc_buff[ol++]=*in;
    }
  }
  tex_esc_buff[ol]=0;
  return tex_esc_buff;
}

// ======== ======== ======== ========
int register_breaks(int issue,char *title,char *problem)
{
  int i;
  char problem_msg[8192];
  for(i=0;problem[i]&&problem[i]!='\r';i++) {
    // Also stop when hitting a non-escaped quote
    
    problem_msg[i]=problem[i];
    problem_msg[i+1]=0;
  }

  fprintf(stderr,"Registering problem: #%d : '%s'\n",
	  issue,problem_msg);
  problem_issues[problem_count]=issue;
  problem_titles[problem_count]=strdup(tex_escape(title));
  problem_descriptions[problem_count]=strdup(tex_escape(problem_msg));
  problem_count++;
  
  return 0;
}

// ======== ======== ======== ========
int show_problem_box(FILE *f,int problem_number)
{
  fprintf(f,"\\begin{table}[H]\n"
	  "\\begin{tabular}{|l|l|l|l|}\n"
	  "\\hline\n"
	  "Issue                                                          & \\href{https://github.com/mega65/mega65-core/issues/%d}{\\#%d} & Resolved? & Y / N / Unsure / Not Applicable \\\\ \\hline\n"
	  "\\begin{tabular}[Hc]{@{}l@{}}Problem\\\\ Description:\\end{tabular} & \\multicolumn{3}{l|}{%s}                           \\\\ \\hline\n"
	  "\\begin{tabular}[Hc]{@{}l@{}}Tester's\\\\ Comments\\end{tabular}    & \\multicolumn{3}{l|}{}                              \\\\ \\hline\n"
	  "\\end{tabular}\n"
	  "\\end{table}\n\n",
	  problem_issues[problem_number],
	  problem_issues[problem_number],
	  problem_descriptions[problem_number]
	  );

  problem_mentioned[problem_number]++;
  
  return 0;
}

// ======== ======== ======== ========
// ======== ======== ======== ========
// ======== ======== ======== ========

/*
Main algorithm is as follows:
- (Step-1) Download the issue-files from the github-api
- (Step-2) Parse through the downloaded issue-files and build up a data-structure in memory
- (Step-3) Generate a TEX-file consisting from IN-put-file and data-structure
- (Step-4) Generate the PDF using a system call to pdflatex
*/

#define TOKEN_LENGTH 41 // 40-chars plus NULL

int main(int argc,char **argv)
{
  // ======== ======== ======== ========
  // Step-1, Download the issue-files from the github-api

  char git_token[TOKEN_LENGTH];
  git_token[0] = 0; // set to empty string

  // get the users token for authenticating with github
  if( access( "./git-token.txt", F_OK ) != -1 )
  {
    printf("Parsing ./git-token.txt\n");
    //
    FILE *gtf=fopen("./git-token.txt","r");
    if (gtf == NULL) {
      fprintf(stderr,"ERROR: Could not read git-token.txt\n");
      exit(-1);
    }

    char line[8192];
    while (fgets(line, 1024, gtf) != NULL) {
      // we allow comments in this git-token file
      if ( line[0] == '#') {
        //printf("skipping comment\n");
      }
      else if (strlen(line) == TOKEN_LENGTH) {
        strncpy(git_token, line, TOKEN_LENGTH);
        //printf("endchar=%d,%c\n", git_token[40],git_token[40]);
        git_token[40] = 0; // replace the 41th char (a CR) with NULL
        printf("Read somethig that is 40 chars -> OK\n");
        break;
      }
      else {
        printf("GIT-TOKEN needs to be 40 chars+NULL\n");
      }

    } // while

    fclose(gtf);
  }
  else
  {
    printf("ERROR, Please put your Auth-token into ./git-token.txt\n");
    exit(1);
  }

  // check the token
  if (strlen(git_token) == 0) {
    printf("ERROR, Could not find the GIT-TOKEN length 40 chars+CR\n");
    exit(-1);
  }
  else
  {
    printf("Got the token \"%s\"\n\n", git_token);
  }

  // ========
  // download the most recent "issues" from the github-api (only if has not been downloaded before)
  if( access( "issues/issues.txt", F_OK ) != -1 )
  {
    // TODO: download if the server copy is newer
    printf("Using local issues/issues.txt\n");
  }
  else
  {
    printf("Downloading latest issues from github-api\n");
    //
    // the following will download ALL issues
    //system("curl -i https://api.github.com/repos/mega65/mega65-core/issues?stat=all > issues.txt");
    //
    // the following will download only the first page of (most recent) issues
    char cmd[1024];
    snprintf(cmd,1024,"curl -H \"Authorization: token %s\" -i https://api.github.com/repos/mega65/mega65-core/issues > issues/issues.txt\n", git_token);
    system(cmd);
  }

  // ========
  // now parse the "issues" file and find the highest issue-number
  // This is so we know how many issues there are.
  FILE *f=fopen("issues/issues.txt","r");
  if (!f) {
    fprintf(stderr,"ERROR: Could not read issues/issues.txt.\n");
    exit(-3);
  }
  char line[8192];
  line[0]=0;
  fgets(line,1024,f);
  while(line[0]) {
    // remove leading whitespace
    while(line[0]&&(line[0]<=' '))
      bcopy(&line[1],&line[0],strlen(line));
    // remove trailling whitespace/CR (??)
    while(line[0]&&(line[strlen(line)-1]<' '))
      line[strlen(line)-1]=0;

    // DEBUG
    //printf("> '%s'\n",line);

    // we want to find the first instance of "number", file looks like
    // <snip>
    //    "id": 607469595,
    //    "node_id": "MDU6SXNzdWU2MDc0Njk1OTU=",
    //    "number": 216,
    //    "title": "Issue with RESTORE key for invoking the freeze menu",
    //    "user": {
    // <snip>

    // the first instance of "number" in issues.txt is
    // the highest issue-number because the issues are listed most recent to oldest(first issue created)
    // so we just read the first line containing an issue-number and get the most recent issue number
    if (!max_issue) {
      sscanf(line,"\"number\": %d",&max_issue);
    }

    // read the next line
    line[0]=0; fgets(line,1024,f);
  }

  fprintf(stderr,"Largest issue number is %d\n\n",max_issue);

  //return 0;



  // ======== ======== ======== ========
  // ======== ======== ======== ========
  // ======== ======== ======== ========



  // Now, iterate through issues and:
  // - download if nessessary
  // - parse the file and pull out important info (ie description and first-post)
  // - check for the existence of ##BREAKS (special tag to indicate how/what to test)
  //
  char cmd[1024]; // char buffer for system calls
  char issue_file[1024]; // char buffer for the filename
  FILE *isf=NULL; // issue file
  //
  // First, lets download the issue-file if nessessary
  //
  for(int issue=1; issue<=max_issue; issue++) {
    //
    snprintf(issue_file,1024,"issues/issue%d.txt",issue);
    printf("== Checking %s", issue_file);
    //
    isf = fopen(issue_file,"r");
    //
    if (isf) {
      // file exists, but we need to check its valid by the first line being "HTTP..."
      char line[1024]; line[0]=0;
      fgets(line,1024,isf);
      // remove trailling whitespace/CR
      while(line[0]&&(line[strlen(line)-1]<' '))
        line[strlen(line)-1]=0;
      //
      if ( strcmp(line,"HTTP/1.1 200 OK") != 0 ) {
        // Close files that didn't fetch correctly (possibly due to rate-limit),
        // and the file will be re-fetched below
        fclose(isf);
        isf=NULL;
        fprintf(stderr,"\nFirst line of '%s' does not match \"HTTP/1.1 200 OK\", -> '%s'\n",issue_file, line);
      }
      //
      // see if there is a newer version of this file on the server
      // - 1), find the datestamp of the local-file
      // - 2), ask server for the file if the server-file is newer
      // - 3), if server-file was newer, replace the old file with the newer file.
      // - 4), if server file was same, ie has not been modified since, throw away the report.
      //
      //== 1)
      char local_timestamp[1024];
      local_timestamp[0] = 0;
      while( line[0] != 0 ) {
        // compare every LINE and when a match is found, get the timestamp
        //sscanf(line,"Last-Modified: %s\n",local_timestamp);
        fgets(line,1024,isf);
        char *pch = strstr(line, "Last-Modified:"); // strstr returns NULL if not found
        if (pch) {
          // yes, this line contains the datestamp, ie
          // Last-Modified: Sun, 16 Aug 2020 10:34:49 GMT<CR><LF><\0>
          // 012345678901234567890
          // 000000000011111111112
          // we want from line[15] onwards
          bcopy(&line[15], &local_timestamp[0], strlen(line));
          // and remove the trailling CR/LF
          local_timestamp[ strlen(local_timestamp)-2 ] = 0;
          break;
        }

      }

      if (local_timestamp[0] == 0) {
        // didnt find the timestamp, something went wrong, -> refetch later by setting "isf" to NULL
        fclose(isf);
        isf=NULL;
        fprintf(stderr,"\nCouldnt find timstamp (Last-Modified:) in '%s', -> refetch\n",issue_file);
      }
      //== 2)
      snprintf(cmd, 1024, "curl --silent -H \"Authorization: token %s\" -H \"If-Modified-Since: %s\" -i https://api.github.com/repos/mega65/mega65-core/issues/%d > api-report.txt\n",
        git_token, local_timestamp, issue);
      //printf("%s\n", cmd);
      system(cmd);
      //== 3)
      FILE *dloaded_file=fopen("api-report.txt","r");
      if (dloaded_file) {
        // something downloaded, lets check the first line of whatever was downloaded:
        // - "HTTP/1.1 200 OK"           if a newer file was downloaded, or
        // - "HTTP/1.1 304 Not Modified" if the local file is the same as the server file
        // so lets check the first line of the downloaded file
        char line[1024]; line[0]=0;
        fgets(line,1024,dloaded_file);
        // remove trailling whitespace/CR
        while(line[0]&&(line[strlen(line)-1]<' '))
          line[strlen(line)-1]=0;
        //
        fclose(dloaded_file); // dont need this opened anymore, and we may move/delete soon
        dloaded_file = NULL;
        //
        if ( strcmp(line,"HTTP/1.1 200 OK") == 0 ) {
          // yes, a newer file was downloaded, so replace the old file with the newly downloaded issue-file
          //== 3)
          printf(" -> downloaded updates, local file is now up-to-date\n");
          snprintf(  cmd,1024,"mv api-report.txt %s\n", issue_file);
          //printf("%s", cmd);
          system(cmd);
        }
        else if ( strcmp(line,"HTTP/1.1 304 Not Modified") == 0 ) {
          // no, the local-file is the same age as the server-file
          //== 4)
          printf(" -> local file is up-to-date\n");
          snprintf(cmd, 1024, "rm api-report.txt\n");
          //printf("%s", cmd);
          system(cmd);
        }
        else {
          printf("\nERROR, did not understand response from server after checking the first line\n");
        }

      }
      else {
          printf("ERROR, could not open \"api-report.txt\".\n");
      }

      //
      // At this stage,
      // we should have the latest/updated version of the issue-file,
      // or we dont have the file at all and so needs fetching (see below).
      //

    } // if


    if (!isf) {
      // Need to refetch it
      fprintf(stderr,"Can't open '%s' (or detected error in file) -- refetching.\n",issue_file);

      printf(  cmd,1024,"curl -H \"Authorization: token %s\" -i https://api.github.com/repos/mega65/mega65-core/issues/%d > %s\n",
        git_token, issue, issue_file);
      snprintf(cmd,1024,"curl -H \"Authorization: token %s\" -i https://api.github.com/repos/mega65/mega65-core/issues/%d > %s\n",
        git_token, issue, issue_file);
      system(cmd);
      // check it worked
      isf=fopen(issue_file,"r");
      if (!isf) {
        fprintf(stderr,"WARNING: Could not fetch issue #%d\n",issue);
      }
      fclose(isf);
      isf = NULL;
    }

  } // for (1 .. max_issue)

  //return 0;

  // ======== ======== ======== ========
  // Step-2, Parse through the downloaded issue files and build up a data-structure in memory
  //
  for(int issue=1; issue<=max_issue; issue++) {
    // Ok, have file, parse it.
    snprintf(issue_file,1024,"issues/issue%d.txt",issue);
    printf("== Parsing %s\n", issue_file);
    //
    isf = fopen(issue_file,"r");
    if (isf) {

      int issue_num;
      char title[8192];
      char body[8192];
      
      char line[8192];
      line[0]=0; fgets(line,8192,isf);

      while(line[0]) {
        // skip whitespace
        while(line[0]&&(line[0]<=' '))
          bcopy(&line[1],&line[0],strlen(line));
        // remove trailling whitespace/CR
        while(line[0]&&(line[strlen(line)-1]<' '))
          line[strlen(line)-1]=0;
        //
        // DEBUG
        //printf("> '%s'\n",line);
        //
        // compare every LINE and when a match is found, gets the issue-number
        sscanf(line,"\"number\": %d",&issue_num);
        //
        // compare every LINE (upto the Xth char) and when a match, parse the remainder of the LINE
        if (!strncmp("\"title\": ",line,9)) {
          //DEBUG only
          //fprintf(stderr,"getting TITLE from '%s' 0..9\n",line);
          parse_string(&line[10],title);
        }
        // compare every LINE (upto the Xth char) and when a match, parse the remainder of the LINE
        if (!strncmp("\"body\": ",line,8)) {
          //DEBUG only
          //fprintf(stderr,"getting BODY from '%s' 0..8\n",line);
          parse_string(&line[9],body);
        }
        //
        line[0]=0; fgets(line,8192,isf);
      }

      fclose(isf);

      // DEBUG only
      if (0) printf("Issue #%d:\ntitle = %s\nbody = %s\n", issue_num, title,body);
      //
      // DEBUG only
      if (0) {
        printf("\n");
        printf("Issue# %d:\n", issue_num);
        printf("title = %s\n", title);
        printf("body = %s\n", body);
      }

      int problem_count=0;
      for (int i=0;body[i];i++) {
        if (!strncmp("\r##BREAKS ",&body[i],10)) {
          register_breaks(issue_num,title,&body[i+10]);
          problem_count++;
        }
      }
      // If no specific problems were registered, then we just need to log the whole
      // issue for now
      if (!problem_count)
        register_breaks(issue_num,title,"Unspecified problem. Please add \\#\\#BREAKS tags via github issue");

    }


  }


  //return 0;

  // ======== ======== ======== ========
  // Step-3, Generate a TEX-file consisting from IN-put-file and data-structure
  //
  // ======== ======== ======== ========
  // ======== ======== ======== ========
  // ======== ======== ======== ========

  // Ok, now we have the set of issues and things that the issues broke,
  // so that we can annotate the test procedure document.
  //
  FILE *of=fopen("testprocedure.tex","w");
  FILE *inf=fopen("test_procedure_in.tex","r");
  if (!of||!inf) {
    fprintf(stderr,"ERROR: Could not open testprocedure.tex or testprocedure_in.tex\n");
    exit(-1);
  }

  // Output Latex header
  fprintf(of,
	  "\\documentclass{article}\n"
	  "\n"
	  "\\title{MEGA65 Test Procedure}\n"
	  // XXX include git commit ID in a \\abstract{} block?
	  "\\usepackage{float}\n"
	  "\\usepackage{hyperref}\n"
	  "\\begin{document}\n"
	  "\\maketitle\n"
	  "\\section*{Test Procedure}\n"
	  "\\begin{enumerate}\n"
	  );

  // now parse the input-file that we have in GIT
  line[0]=0; fgets(line,8192,inf);
  while(line[0]) {
    //
    int queued_problems[MAX_PROBLEMS];
    //
    for(int i=0;line[i];i++) {
      //
      // check for "#["
      if (line[i]=='#') {
	if (line[i+1]=='[') {
	  // Named problem
	  char *p=&line[i+2];
	  int len=0;
	  int found=0;
	  while(p[len]&&p[len]!=']') len++;
	  printf("named problem at '%s', len=%d\n",
		 p,len);
	  for(int j=0;j<problem_count;j++) {
	    if (!strncmp(problem_descriptions[j],p,len)) {
	      show_problem_box(of,j);
	      found=1;
	    }
	  }
	  if (!found) {
	    int z=p[len]; p[len]=0;
	    fprintf(of,"ERROR: Problem ``%s'' does not appear in issues. Please check spelling and punctuation are exactly matching the text in the \\#\\#BREAKS directive in the issue body.\n",p);
	    p[len]=z;
	  }
	  i+=1+len+1;	  
	  
	} else {
	  // Issue number: Show all problems that the issue references
	  char num[16];
	  int nlen=0;
	  while(isdigit(line[i+1+nlen])&&(nlen<16)) {
	    num[nlen++]=line[i+1+nlen];	    
	  }
	  i+=nlen;
	  num[nlen]=0;
	  int issue_num=atoi(num);
	  for(int j=0;j<problem_count;j++) {
	    if (problem_issues[j]==issue_num)
	      show_problem_box(of,j);
	  }
	}
      }
      else
        // current character is not a "#"
        fprintf(of,"%c",line[i]);

    } // for loop
    
    line[0]=0; fgets(line,8192,inf);

  } // while loop

  fprintf(of,"\\item End of procedure.\n");
  
  fprintf(of,"\\end{enumerate}\n");


  int missed_count=0;
  for(int i=0;i<problem_count;i++) {
    if (!problem_mentioned[i]) {

      if (!missed_count)
        fprintf(of,"\\section*{Issues not (yet) included in the test procedure}\n");
      show_problem_box(of,i);
      missed_count++;

    }
  }
  
  fprintf(of,"\\end{document}\n");
  
  fclose(of);
  fclose(inf);

  // ======== ======== ======== ========
  // Step-4, Generate the PDF using a system call to pdflatex
  //
  printf("\n");
  printf("========================================\n");
  printf("= Making system call \"pdflatex\"\n");
  printf("========================================\n\n");
  //
  system("pdflatex testprocedure");


}
