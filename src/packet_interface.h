#ifndef __PACKET_INTERFACE_H_
#define __PACKET_INTERFACE_H_

#include <stddef.h> /* size_t */
#include <stdint.h> /* uintx_t */
#include <unistd.h>
#include <stdio.h>  /* ssize_t */

/* Taille maximale permise pour le payload */
#define MAX_PAYLOAD_SIZE 512
/* Taille maximale de Window */
#define MAX_WINDOW_SIZE 31

#define member_size(type, member) sizeof((((type *) NULL)->member))

#define PKT_MAX_HEADERLEN offsetof(pkt_t, payload)
#define PKT_MIN_HEADERLEN PKT_MAX_HEADERLEN - 2
#define PKT_CRC1LEN member_size(pkt_t, crc1)
#define PKT_FOOTERLEN (sizeof(pkt_t) - offsetof(pkt_t, crc2))
#define PKT_MIN_LEN PKT_MIN_HEADERLEN
#define PKT_MAX_LEN (PKT_MAX_HEADERLEN + MAX_PAYLOAD_SIZE + PKT_FOOTERLEN)
#define PKT_TR_BIT_OFFSET 2

#define PKT_TIMESTAMP 0xdeadbeef

/* Raccourci pour struct pkt */
typedef struct pkt pkt_t;

/* Types de paquets */
typedef enum {
	PTYPE_FEC = 0,
	PTYPE_DATA = 1,
	PTYPE_ACK = 2,
	PTYPE_NACK = 3,
} ptypes_t;


/* Valeur de retours des fonctions */
typedef enum {
	PKT_OK = 0,     /* Le paquet a été traité avec succès */
	E_TYPE,         /* Erreur liée au champs Type */
	E_TR,           /* Erreur liee au champ TR */
	E_LENGTH,       /* Erreur liée au champs Length  */
	E_CRC,          /* CRC invalide */
	E_WINDOW,       /* Erreur liée au champs Window */
	E_SEQNUM,       /* Numéro de séquence invalide */
	E_NOMEM,        /* Pas assez de mémoire */
	E_NOHEADER,     /* Le paquet n'a pas de header (trop court) */
	E_UNCONSISTENT, /* Le paquet est incohérent */
} pkt_status_code;

/* Alloue et initialise une struct pkt
 * @return: NULL en cas d'erreur */
pkt_t* pkt_new();
/* Libère le pointeur vers la struct pkt, ainsi que toutes les
 * ressources associées */
void pkt_del(pkt_t*);

/*
 * Retourne la longueur du header en bytes.
 */
ssize_t predict_header_length(const pkt_t *pkt);

/*
 * Décode des données reçues et crée une nouvelle structure pkt.
 * Le paquet reçu est en network byte-order.
 * La fonction vérifie que:
 * - Le CRC32 du header recu est le même que celui decode a la fin
 *   du header (en considerant le champ TR a 0)
 * - S'il est present, le CRC32 du payload recu est le meme que celui
 *   decode a la fin du payload
 * - Le type du paquet est valide
 * - La longueur du paquet et le champ TR sont valides et coherents
 *   avec le nombre d'octets recus.
 *
 * @data: L'ensemble d'octets constituant le paquet reçu
 * @len: Le nombre de bytes reçus
 * @pkt: Une struct pkt valide
 * @post: pkt est la représentation du paquet reçu
 *
 * @return: Un code indiquant si l'opération a réussi ou représentant
 *         l'erreur rencontrée.
 */
pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt);

/*
 * Encode une struct pkt dans un buffer, prêt à être envoyé sur le réseau
 * (c-à-d en network byte-order), incluant le CRC32 du header et
 * eventuellement le CRC32 du payload si celui-ci est non nul.
 * La fonction pkt doit calculer les champs CRC elle-même, car
 * ils ne sont pas nécessairements présents dans pkt.
 *
 * @pkt: La structure à encoder
 * @buf: Le buffer dans lequel la structure sera encodée
 * @len: La taille disponible dans le buffer
 * @len-POST: Le nombre de d'octets écrit dans le buffer
 * @return: Un code indiquant si l'opération a réussi ou E_NOMEM si
 *         le buffer est trop petit.
 */
pkt_status_code pkt_encode(const pkt_t*, char *buf, size_t *len);

/* Accesseurs pour les champs toujours présents du paquet.
 * Les valeurs renvoyées sont toutes dans l'endianness native
 * de la machine!
 */
ptypes_t pkt_get_type     (const pkt_t*);
uint8_t  pkt_get_tr       (const pkt_t*);
uint8_t  pkt_get_window   (const pkt_t*);
uint8_t  pkt_get_seqnum   (const pkt_t*);
uint16_t pkt_get_length   (const pkt_t*);
uint32_t pkt_get_timestamp(const pkt_t*);
uint32_t pkt_get_crc1      (const pkt_t*);
/* Renvoie un pointeur vers le payload du paquet, ou NULL s'il n'y
 * en a pas.
 */
const char* pkt_get_payload(const pkt_t*);
/* Renvoie le CRC2 dans l'endianness native de la machine. Si
 * ce field n'est pas present, retourne 0.
 */
uint32_t pkt_get_crc2(const pkt_t*);

/* Setters pour les champs obligatoires du paquet. Si les valeurs
 * fournies ne sont pas dans les limites acceptables, les fonctions
 * doivent renvoyer un code d'erreur adapté.
 * Les valeurs fournies sont dans l'endianness native de la machine!
 */
pkt_status_code pkt_set_type     (pkt_t*, const ptypes_t type);
pkt_status_code pkt_set_tr       (pkt_t*, const uint8_t tr);
pkt_status_code pkt_set_window   (pkt_t*, const uint8_t window);
pkt_status_code pkt_set_seqnum   (pkt_t*, const uint8_t seqnum);
pkt_status_code pkt_set_length   (pkt_t*, const uint16_t length);
pkt_status_code pkt_set_timestamp(pkt_t*, const uint32_t timestamp);
pkt_status_code pkt_set_crc1      (pkt_t*, const uint32_t crc1);
/* Défini la valeur du champs payload du paquet.
 * @data: Une succession d'octets représentants le payload
 * @length: Le nombre d'octets composant le payload
 * @POST: pkt_get_length(pkt) == length */
pkt_status_code pkt_set_payload(pkt_t*,
                                const char *data,
                                const uint16_t length);
/* Setter pour CRC2. Les valeurs fournies sont dans l'endianness
 * native de la machine!
 */
pkt_status_code pkt_set_crc2(pkt_t*, const uint32_t crc2);


#endif  /* __PACKET_INTERFACE_H_ */