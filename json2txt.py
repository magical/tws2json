#!/usr/bin/env python3
import io
import json
import sys

def main():
    j = json.load(sys.stdin)
    for sol in j['solutions']:
        if 'moves' in sol:
            print(sol['moves'])
            print(' '.join(parsemoves(sol['moves'])))

DIGITS = '0123456789'
UPPERMOVES = "UDLR"
LOWERMOVES = "udlr"
WAIT = ",."
MOUSE = "*"
EOF = ""

# states
new_move = 1
digit_or_move  = 2
mouse  = 6
mouse1a = 8
mouse1b = 9 
mouse2a =10
mouse2b =11
moveupper = 12
movelower = 13
diagonalupper = 14
diagonallower = 15


def parsemoves(s):
    r = io.StringIO(s)
    moves = []
    count = 0
    move = ""
    state = new_move
    column = 0

    def flush_move():
        nonlocal move
        for _ in range(count):
            moves.append(move)
        move = ""

    while True:
        if state == 'error':
            print("error at column", column)
            break

        if state == 'flush':
            flush_move()
            state = new_move

        column += 1

        c = r.read(1)
        #print(c, state)

#    new_move -> digit_or_move [label="1-9"];
#    new_move -> mouse [label="*"];
#    new_move -> moveupper [label="UDLR"];
#    new_move -> movelower [label="udlr"];
#    new_move -> new_move [label="<space>"];
#
        if state == new_move:
            count = 1
            if c in DIGITS:
                count = DIGITS.index(c)
                state = digit_or_move
            elif c == MOUSE:
                move = '*'
                state = mouse
            elif c in UPPERMOVES:
                move = c
                state = moveupper
            elif c in LOWERMOVES:
                move = c
                state = movelower
            elif c in WAIT:
                move = c
                state = 'flush'
            elif c == ' ':
                pass
            elif c == EOF:
                pass
            else:
                state = 'error'
#    digit_or_move -> digit_or_move [label="0-9"];
#    digit_or_move -> moveupper [label="UDLR"];
#    digit_or_move -> movelower [label="udlr"];
#    digit_or_move -> new_move [label=", .", style=bold];
        elif state == digit_or_move:
            if c in DIGITS:
                count = count*10 + DIGITS.index(c)
            elif c in UPPERMOVES:
                move = c
                state = moveupper
            elif c in LOWERMOVES:
                move = c
                state = movelower
            elif c in WAIT:
                move = c
                state = 'flush'
            else:
                state = 'error'

#    moveupper -> diagonalupper [label="+"];
#    moveupper -> digit_or_move [label="1-9", style=dashed];
#    moveupper -> mouse [label="*", style=dashed];
#    moveupper -> moveupper [label="UDLR", style=dashed];
#    moveupper -> movelower [label="udlr", style=dashed];
        elif state == moveupper:
            if c == '+':
                state = diagonalupper
            else:
                flush_move()

                count = 1
                if c in DIGITS:
                    count = DIGITS.index(c)
                    state = digit_or_move
                elif c == MOUSE:
                    state = mouse
                elif c in UPPERMOVES:
                    move = c
                    state = moveupper
                elif c in LOWERMOVES:
                    move = c
                    state = movelower
                elif c in WAIT:
                    move = c
                    state = 'flush'
                elif c == ' ':
                    state = new_move
                elif c == EOF:
                    flush_move()
                else:
                    state = 'error'

#    movelower -> diagonallower [label="+"];
#    movelower -> digit_or_move [label="1-9", style=dashed];
#    movelower -> mouse [label="*", style=dashed];
#    movelower -> moveupper [label="UDLR", style=dashed];
#    movelower -> movelower [label="udlr", style=dashed];
        elif state == movelower:
            if c == '+':
                state = diagonallower
            else:
                flush_move()

                count = 1
                if c in DIGITS:
                    count = DIGITS.index(c)
                    state = digit_or_move
                elif c == MOUSE:
                    state = mouse
                elif c in UPPERMOVES:
                    move = c
                    state = moveupper
                elif c in LOWERMOVES:
                    move = c
                    state = movelower
                elif c in WAIT:
                    move = c
                    state = 'flush'
                elif c == ' ':
                    state = new_move
                elif c == EOF:
                    flush_move()
                else:
                    state = 'error'

#    diagonalupper -> new_move [label="UDLR", style=bold];
        elif state == diagonalupper:
            if c in UPPERMOVES:
                move += c
                state = 'flush'
            else:
                state = 'error'

#    diagonallower -> new_move [label="udlr", style=bold];
        elif state == diagonallower:
            if c in LOWERMOVES:
                move += c
                state = 'flush'
            else:
                state = 'error'

#    mouse -> new_move [label="."];
#    mouse -> mouse1a [label="2-9"];
#    mouse -> mouse1b [label="UDLR"];
        elif state == mouse:
            x = 0
            y = 0
            digit1 = 0
            digit2 = 0
            dir1 = None
            dir2 = None
            move = "*"
            if c == '.':
                x = 0
                y = 0
                state = 'flush'
            elif c in DIGITS:
                digit1 = int(c)
                state = mouse1a
            elif c in UPPERMOVES:
                digit1 = 1
                dir1 = c
                state = mouse1b
            else:
                state = 'error'

#    mouse1a -> mouse1b [label="UDLR"];
        elif state == mouse1a:
            if c in UPPERMOVES:
                dir1 = c
                state = mouse1b
            else:
                state = 'error'
        

#    mouse1b -> mouse2a [label=";"];
#    mouse1b -> digit_or_move [label="1-9", style=dashed];
#    mouse1b -> mouse [label="*", style=dashed];
#    mouse1b -> moveupper [label="UDLR", style=dashed];
#    mouse1b -> movelower [label="udlr", style=dashed];
        elif state == mouse1b:
            if c == ';':
                state = mouse2a
            else:
                flush_move()

                count = 1
                if c in DIGITS:
                    count = DIGITS.index(c)
                    state = digit_or_move
                elif c == MOUSE:
                    state = mouse
                elif c in UPPERMOVES:
                    move = c
                    state = moveupper
                elif c in LOWERMOVES:
                    move = c
                    state = movelower
                elif c in WAIT:
                    move = c
                    state = 'flush'
                elif c == ' ':
                    state = new_move
                elif c == EOF:
                    flush_move()
                else:
                    state = 'error'

#    mouse2a -> mouse2b [label="2-9"];
#    mouse2a -> new_move [label="UDLR", style=bold];
        elif state == mouse2a:
            if c in DIGITS:
                digit2 = int(c)
                state = mouse2b
            elif c in UPPERMOVES:
                digit2 = 1
                dir2 = c
                state = 'flush'
            else:
                state = 'error'

        if not c:
            break
            

    return moves


main()
