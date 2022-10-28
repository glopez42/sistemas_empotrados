#define bit_es_igual_uno(dato, nbit) (dato & (1<<nbit))
#define bit_es_igual_cero(dato, nbit) (~dato & (1<<nbit))

#define bit_fija_a_uno(dato, nbit) (dato | (1<<nbit))
#define bit_fija_a_cero(dato, nbit) (dato & ~(1<<nbit))

#define bit_invierte(dato, nbit) (dato ^ (1<<nbit))

#define bits_extrae(dato, nbit, tam_bits) ((dato>>nbit) & ((1<<tam_bits)-1))

#define bits_inserta(dato, nbit, tam_bits, inserto) ((dato & ~(((1<<tam_bits)-1) << nbit)) | ((inserto & ((1<<tam_bits)-1)) << nbit)) 
