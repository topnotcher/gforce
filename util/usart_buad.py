
TARGET_BAUD = 1515.762
F_CPU = 32000000

def p_error(a,e):
    return abs(a-e)/a*100

def get_bsel(bscale):
    return round( F_CPU / ( (2**bscale) *16 * TARGET_BAUD ) - 1 )

def get_baud(bscale, bsel):
    return F_CPU/(2**bscale*16*(bsel+1))
            
for bscale in range(-7,7):
    bsel = get_bsel(bscale)
    baud = get_baud(bscale,round(bsel))

    err = p_error(TARGET_BAUD,baud)

    print("BSEL: %d, BSCALE: %d, ERR: %.5f%%" % (bsel,bscale, err) )
