    .file   "game.c"
    .text
    .global changeScore
    
changeScore:
    cmp #1, r13
    jz p1score
    cmp #2, r13
    jz p2score
    
p1score:
    add.b #1, 0(r12)
    jmp end
    
p2score:
    add.b #1, 1(r12)
    jmp end

end:
    POP R0
