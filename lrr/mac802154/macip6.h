#ifndef	MACIP6_H
#define	MACIP6_H

#define	MACIP6_RPL_DIS		1
#define	MACIP6_RPL_DIO		2
#define	MACIP6_RPL_DAO		3
#define	MACIP6_RPL_DAOACK	4

#define	MACIP6_PING_REQUEST	10
#define	MACIP6_PING_REPLY	11

#define	MACIP6_ROUTER_SOLI	20

int	mac802154_ip6type(mac802154_t *mf);
char	*mac802154_ip6typetxt(int type);
#endif
