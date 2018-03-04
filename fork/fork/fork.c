//
//  fork.c
//  fork
//
//  Created by Adam Bezák on 3.3.18.
//  Copyright © 2018 Adam Bezák. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1 /* XPG 4.2 - needed for WCOREDUMP() */

#define ERROR -1
#define SUCCESS 0

/* ARGSUSED */
int main(int argc, char *argv[])
{
    printf("test");
    return 0;
}

