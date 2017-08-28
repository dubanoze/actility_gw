#ifndef NOSSL
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "rtlbase.h"


void	lrr_keyInit()
{
#if	0
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	OPENSSL_config(NULL);
#endif
}

int lrr_keyEnc(unsigned char *plaintext, int plaintext_len, unsigned char *key,
unsigned char *iv, unsigned char *ciphertext,int maxlen,int aschex)
{
	EVP_CIPHER_CTX *ctx;
	int	len;
	int	ciphertext_len;
	int	ret;
	int	sz;

	unsigned char	keybin[128/8];
	unsigned char	ivbin[128/8];

	unsigned char	intmpbin[2048];
	unsigned char	tmpbin[2048];

	if	(!plaintext)			return	-1;
	if	(plaintext_len < 0)	// input in ascii hex format
	{
		plaintext_len	= 512;
		rtl_strToBin((char *)plaintext,intmpbin,&plaintext_len);
		plaintext	= intmpbin;
	}
	if	(plaintext_len < 0 || plaintext_len > 256)		
						return	-2;
	if	(key && strlen((char *)key) != 32)	return	-3;
	if	(iv && strlen((char *)iv) != 32)	return	-4;
	if	(!ciphertext)			return	-5;

	memset	(keybin,0,sizeof(keybin));
	if	(key)
	{
		sz	= sizeof(keybin);
		rtl_strToBin((char *)key,keybin,&sz);
		if	(sz != sizeof(keybin))	return	-6;
	}

	memset	(ivbin,0,sizeof(ivbin));
	if	(iv)
	{
		sz	= sizeof(ivbin);
		rtl_strToBin((char *)iv,ivbin,&sz);
		if	(sz != sizeof(ivbin))	return	-7;
	}

	ctx	= EVP_CIPHER_CTX_new();
	if	(!ctx)			return	-10;


	ret	= EVP_EncryptInit(ctx,EVP_aes_128_cbc(),keybin,ivbin);
	if	(1 != ret)		return	-11;
	ret	= EVP_EncryptUpdate(ctx,tmpbin,&len,plaintext,plaintext_len);
	if	(1 != ret)		return	-12;
	ciphertext_len	= len;
	ret	= EVP_EncryptFinal(ctx,tmpbin + len,&len);
	if	(1 != ret)		return	-13;
	ciphertext_len	+= len;
	EVP_CIPHER_CTX_free(ctx);

	if	(ciphertext_len <= 0)	return	-14;

	if	(aschex)
	{
		rtl_binToStr(tmpbin,ciphertext_len,(char *)ciphertext,maxlen);
		ciphertext_len	= ciphertext_len * 2;
		if	(strlen((char *)ciphertext) != ciphertext_len)
					return	-15;
	}
	else
	{
		if	(ciphertext_len > maxlen)
					return	-16;
		memcpy	(ciphertext,tmpbin,ciphertext_len);
	}
	return ciphertext_len;
}

int lrr_keyDec(unsigned char *ciphertext,int ciphertext_len,unsigned char *key,
  unsigned char *iv, unsigned char *plaintext,int maxlen,int aschex)
{
	EVP_CIPHER_CTX *ctx;
	int 	plaintext_len;
	int	len;
	int	ret;
	int	sz;

	unsigned char	keybin[128/8];
	unsigned char	ivbin[128/8];

	unsigned char	intmpbin[2048];
	unsigned char	tmpbin[2048];

	if	(!ciphertext)			return	-1;
	if	(ciphertext_len < 0)	// input in ascii hex format
	{
		ciphertext_len	= 512;
		rtl_strToBin((char *)ciphertext,intmpbin,&ciphertext_len);
		ciphertext	= intmpbin;
	}
	if	(ciphertext_len <= 0 || ciphertext_len > 256)		
						return	-2;
	if	(key && strlen((char *)key) != 32)	return	-3;
	if	(iv && strlen((char *)iv) != 32)	return	-4;
	if	(!plaintext)			return	-5;

	memset	(keybin,0,sizeof(keybin));
	if	(key)
	{
		sz	= sizeof(keybin);
		rtl_strToBin((char *)key,keybin,&sz);
		if	(sz != sizeof(keybin))	return	-6;
	}

	memset	(ivbin,0,sizeof(ivbin));
	if	(iv)
	{
		sz	= sizeof(ivbin);
		rtl_strToBin((char *)iv,ivbin,&sz);
		if	(sz != sizeof(ivbin))	return	-7;
	}

	ctx	= EVP_CIPHER_CTX_new();
	if	(!ctx)			return	-10;


	ret	= EVP_DecryptInit(ctx,EVP_aes_128_cbc(),keybin,ivbin);
	if	(1 != ret)		return	-11;

	ret	= EVP_DecryptUpdate(ctx,tmpbin,&len,ciphertext,ciphertext_len);
	if	(1 != ret)		return	-12;
	plaintext_len = len;

	ret	= EVP_DecryptFinal(ctx,tmpbin+len,&len);
	plaintext_len += len;

	if	(plaintext_len < 0)	return	-14;

	if	(aschex)
	{
		rtl_binToStr(tmpbin,plaintext_len,(char *)plaintext,maxlen);
		plaintext_len	= plaintext_len * 2;
		if	(strlen((char *)plaintext) != plaintext_len)
					return	-15;
	}
	else
	{
		if	(plaintext_len > maxlen)
					return	-16;
		memcpy	(plaintext,tmpbin,plaintext_len);
	}

	EVP_CIPHER_CTX_free(ctx);

	return	plaintext_len;
}

#ifdef	MAIN
int	main(int argc,char *argv[])
{
	int	opt;
	int	verbose	= 0;
	int	encrypt	= 1;
	int	decrypt	= 0;
	unsigned	char	buffkey[1024];
	unsigned	char	bufftext[1024];
//	unsigned	char	*plaintext = "support";
	unsigned	char	*plaintext = NULL;
	int	lg;
//	unsigned	char	*key	= "00000000000000000000000000000000";
	unsigned	char	*key	= NULL;
	unsigned	char	*iv	= NULL;
	unsigned	char	ahxplain[512];
	unsigned	char	ahxcrypted[512];
	unsigned	char	bincrypted[512];
	unsigned	char	uncrypted[512];
	int	ret;
	int	aschex;
	int	lgbin;
	char	*pt;

	while	((opt=getopt(argc,argv,"vd")) != -1)
	{
		switch	(opt)
		{
		case	'v'	: verbose	= 1; 			break;
		case	'd'	: encrypt	= 0;	decrypt	= 1; 	break;
		}
	}

	if	(argc > 1)
		verbose	= 1;

	memset	(buffkey,'0',sizeof(buffkey));
	printf	("enter key (128b ascii hexa format [0-9a-f][0-9a-f]x16):\n");
	printf	(" 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5\n");
	ret	= scanf	("%[^\n]",buffkey);
	if	(ret == EOF)
		exit(1);
	getchar();	// get \n
	buffkey[32]	= '\0';
	printf	("%s\n",buffkey);
	key	= buffkey;

	lrr_keyInit();

	while	(encrypt)
	{
	fflush(stdin);
	memset	(bufftext,0,sizeof(bufftext));
	printf	("enter value to encrypt (max [0-128]bytes):\n");
	ret	= scanf("%[^\n]",bufftext);
	if	(ret == EOF)
		break;
	getchar();	// get \n
	bufftext[128]	= '\0';
	pt	= strchr((char *)bufftext,'\n'); if (pt) *pt = '\0';
	pt	= strchr((char *)bufftext,'\r'); if (pt) *pt = '\0';
	printf	("'%s'\n",bufftext);
	plaintext	= bufftext;
	lg	= strlen((char *)plaintext);



	ret	= lrr_keyEnc(plaintext,lg,key,iv,bincrypted,500,aschex=0);
	if	(ret <= 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }
	if	(verbose)
	printf	("lrr_keyEnc() to binary ret=%d\n",ret);
	lgbin	= ret;
	if	(verbose)
	{
		unsigned char	c;
		for	(ret = 0 ; ret < lgbin && ret < 16 ; ret++)
		{
			c	= bincrypted[ret];
			printf("%02x(%c) ",c,(c>=' ' && c<=0x7f)?c:'.');
		}
		printf	("\n");
	}

	rtl_binToStr(plaintext,strlen((char *)plaintext),(char *)ahxplain,500);
	ret	= lrr_keyEnc(ahxplain,-1,key,iv,ahxcrypted,500,aschex=1);
	if	(ret <= 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }
	if	(verbose)
	printf	("lrr_keyEnc() from/to aschex ret=%d '%s'\n",ret,ahxcrypted);

	ret	= lrr_keyEnc(plaintext,lg,key,iv,ahxcrypted,500,aschex=1);
	if	(ret <= 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }
	if	(verbose)
	printf	("lrr_keyEnc() to aschex ret=%d '%s'\n",ret,ahxcrypted);


	memset	(uncrypted,0,sizeof(uncrypted));
	ret	= lrr_keyDec(ahxcrypted,-1,key,iv,uncrypted,500,aschex=1);
	if	(verbose)
	printf	("lrr_keyDec() from/to aschex ret=%d '%s'\n",ret,uncrypted);
	ret	= strcmp((char *)ahxplain,(char *)uncrypted);
	if	(ret != 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }

	memset	(uncrypted,0,sizeof(uncrypted));
	ret	= lrr_keyDec(ahxcrypted,-1,key,iv,uncrypted,500,aschex=0);
	if	(verbose)
	printf	("lrr_keyDec() from aschex ret=%d '%s'\n",ret,uncrypted);
	ret	= strcmp((char *)plaintext,(char *)uncrypted);
	if	(ret != 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }

	memset	(uncrypted,0,sizeof(uncrypted));
	ret	= lrr_keyDec(bincrypted,lgbin,key,iv,uncrypted,500,aschex=0);
	if	(verbose)
	printf	("lrr_keyDec() from binary ret=%d '%s'\n",ret,uncrypted);
	ret	= strcmp((char *)plaintext,(char *)uncrypted);
	if	(ret != 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }

	printf	("\n\nuncrypted='%s' => crypted='%s'\n",
		plaintext,ahxcrypted);

	}

	while	(decrypt)
	{
	fflush(stdin);
	memset	(ahxcrypted,0,sizeof(ahxcrypted));
	printf	("enter value to decrypt (max [0-256]bytes):\n");
	ret	= scanf("%[^\n]",ahxcrypted);
	if	(ret == EOF)
		break;
	getchar();	// get \n
	ahxcrypted[256]	= '\0';
	pt	= strchr((char *)ahxcrypted,'\n'); if (pt) *pt = '\0';
	pt	= strchr((char *)ahxcrypted,'\r'); if (pt) *pt = '\0';
	if	(strlen((char *)ahxcrypted) == 0)	continue;
	printf	("'%s'\n",ahxcrypted);


	memset	(uncrypted,0,sizeof(uncrypted));
	ret	= lrr_keyDec(ahxcrypted,-1,key,iv,uncrypted,500,aschex=1);
	if	(ret < 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }

	printf	("\n\ncrypted='%s' => uncryptedhex ='%s'\n",
		ahxcrypted,uncrypted);

	memset	(uncrypted,0,sizeof(uncrypted));
	ret	= lrr_keyDec(ahxcrypted,-1,key,iv,uncrypted,500,aschex=0);
	if	(ret < 0)
		{ printf("error ret=%d line=%d\n",ret,__LINE__); exit(1); }

	uncrypted[ret]	= '\0';
	printf	("crypted='%s' => uncrypted ='%s'\n",
		ahxcrypted,uncrypted);

	}

	exit(0);
}
#endif
#endif
