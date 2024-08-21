#!/usr/bin/python3

import sys

if len(sys.argv) != 3:
    print("Usage: cdb_class_sort.py INFILE.JSON OUTFILE.JSON")
    raise SystemExit(1)

classes = {}
classBuf = []
className = None
isClass = False
readingClass = False

f = open(sys.argv[1], "r")
for l in f:
    if l == "\t{\n" and not readingClass:
        readingClass = True
    if readingClass:
        classBuf += [l]
        if l.startswith("\t\t\"Name\": \""):
            className = l[11:-3]
        if l == "\t\t\"Type\": \"<class>\",\n":
            isClass = True
        elif l.startswith("\t\t\"Type\": \""):
            break
        if l == "\t},\n":
            if className and isClass:
                classes[className] = classBuf
            classBuf = []
            className = None
            isClass = False
            readingClass = False
f.close()

classNames = []
for i in classes.keys():
    classNames += [i]
classNames.sort()

f = open(sys.argv[2], "w")
for i in classNames:
    for l in classes[i]:
        f.write(l)
f.close()

