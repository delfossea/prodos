#include<stdio.h>
#include<string.h>
#include<strings.h>

int
main (int argc, char *argv[])
{
  FILE *fp;
  char filename[64];
  char type[64];
  bzero (filename, 64);
  bzero (type, 64);
  type[1] = 6;
  type[3] = 32;
  strcpy (filename, argv[1]);
  fp = fopen (".prodosdir", "a");
  fwrite (filename, 1, 64, fp);
  fwrite (type, 1, 64, fp);
  fclose (fp);
}
