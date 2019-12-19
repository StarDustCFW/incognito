#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <switch.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
using namespace std;
//static char version[32] = "1.4-5";

//ask to the switch for the serial
char *SwitchIdent_GetSerialNumber(void) {
	setInitialize();
    setsysInitialize();
	Result ret = 0;
	static char serial[0x19];
	if (R_FAILED(ret = setsysGetSerialNumber(serial)))
		printf("setsysGetSerialNumber() failed: 0x%x.\n\n", ret);
//	if(strlen(serial) == 0) {sprintf(serial, "XAW00000000000");}
	setsysExit();
	return serial;
}


//traduction
bool isSpanish()
{
			setInitialize();
			u64 lcode = 0;
			SetLanguage lang;
			setGetSystemLanguage(&lcode);
			setMakeLanguage(lcode, &lang);
				switch(lang)
				{
					case 5:
					case 14:
					return true;
					   break;
					default:
					return false;
						break;
				}
			setsysExit();
		return false;
}

bool mainMenu();
bool restore();
bool fileExists(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return true;
	}
	return false;
}

//force this config to evoid a crash
void forceconfig()
{
	fsInitialize();
	if(!fileExists("sdmc:/StarDust/StarDustV.txt"))
	{
		remove("sdmc:/atmosphere/config/system_settings.ini");
		FILE* c = fopen("sdmc:/atmosphere/config/system_settings.ini", "w");
		fprintf (c, "\n[eupld]\n");
		fprintf (c, "upload_enabled = u8!0x0\n");
		fprintf (c, "\n[usb]\n");
		fprintf (c, "usb30_force_enabled = u8!0x0\n");
		fprintf (c, "\n[ro]\n");
		fprintf (c, "ease_nro_restriction = u8!0x0\n");
		fprintf (c, "\n[atmosphere]\n");
		fprintf (c, "fatal_auto_reboot_interval = u64!0x0\n");
		fprintf (c, "power_menu_reboot_function = str!payload\n");
		fprintf (c, "dmnt_cheats_enabled_by_default = u8!0x1\n");
		fprintf (c, "dmnt_always_save_cheat_toggles = u8!0x0\n");
		fprintf (c, "enable_hbl_bis_write = u8!0x1\n");
		fprintf (c, "enable_hbl_cal_read = u8!0x1\n");
		fprintf (c, "fsmitm_redirect_saves_to_sd = u8!0x0\n");
		fprintf (c, "\n[hbloader]\n");
		fprintf (c, "applet_heap_size = u64!0x0\n");
		fprintf (c, "applet_heap_reservation_size = u64!0x8000000\n");
		fclose(c);
		fsExit();
	}
}

class Incognito
{
public:

	Incognito()
	{		
		if (fsOpenBisStorage(&m_sh, FsBisPartitionId_CalibrationBinary))
		{
			if(isSpanish())
			printf("\x1b[31;1merror:\x1b[0m no se pudo abrir la partición Prodinfo.\n");
			else
			printf("\x1b[31;1merror:\x1b[0m failed to open Prodinfo partition.\n");
			m_open = false;
		}
		else
		{
			m_open = true;
		}
	}
	
	~Incognito()
	{
		close();
	}

	bool close()
	{
		if (isOpen())
		{
			fsStorageClose(&m_sh);
			return true;
		}
		return false;
	}

	bool isOpen()
	{
		return m_open;
	}

	char* backupFileName()
	{
		static char filename[32] = "sdmc:/backup/prodinfo.bin";
		if (!fileExists(filename))
		{
			mkdir("sdmc:/backup/", 777);
			return filename;
		}
		else
		{
			for (int i = 0; i < 99; i++)
			{
				sprintf(filename, "sdmc:/backup/prodinfo-%d.bin", i);

				if (!fileExists(filename))
				{
				return filename;
				}
			}
		}
		return filename;
	}

	bool backup()
	{
		const char* fileName = backupFileName();

		if (!fileName)
		{
			printf("\x1b[31;1merror:\x1b[0m failed to get backup file name\n");
			return false;
		}

		u8* buffer = new u8[size()];

		if (fsStorageRead(&m_sh, 0x0, buffer, size()))
		{
			printf("\x1b[31;1merror:\x1b[0m failed reading Prodinfo\n");

			delete buffer;
			return false;
		}
				FsFileSystem save;
			    fsOpenBisFileSystem(&save, FsBisPartitionId_SafeMode, "");
                fsdevMountDevice("safemode2", save);
				
			FILE* f = fopen(fileName, "wb+");

			if (!f)
			{
				printf("\x1b[31;1merror:\x1b[0m failed to open %s for writing\n", fileName);

				delete buffer;
				return false;
			}

		fwrite(buffer, 1, size(), f);
		//make a Prodinfo backup on nand
		if (!fileExists("safemode2:/prodinfo.bin"))
		{
			FILE* g = fopen("safemode2:/prodinfo.bin", "wb+");		
			fwrite(buffer, 1, size(), g);
			fclose(g);
			fsdevCommitDevice("safemode2");
		}
		delete buffer;
		
		fsdevUnmountDevice("safemode2");
		fsFsClose(&save);
		
		fclose(f);

		printf("saved backup to %s\n", fileName);
		return true;
	}

	s64 size()
	{
		s64 s = 0;
		fsStorageGetSize(&m_sh, &s);
		return s;
	}

	bool clean()
	{

	
		if (!backup())
		{
			return false;
		}

		const char junkSerial[] = "XAW00000000000";

		if (fsStorageWrite(&m_sh, 0x0250, junkSerial, strlen(junkSerial)))
		{
			printf("\x1b[31;1merror:\x1b[0m failed writing serial\n");
			if(isSpanish())
			printf("\x1b[36;1mEl CFW esta bloqueando el acceso al prodinfo\x1b[0m\n");
			else
			printf("\x1b[36;1mThe CFW is blocking the access to the prodinfo\x1b[0m\n");
			return false;
		}

		erase(0x0AE0, 0x800); // client cert
		erase(0x3AE0, 0x130); // private key
		erase(0x35E1, 0x006); // deviceId
		erase(0x36E1, 0x006); // deviceId
		erase(0x02B0, 0x180); // device cert
		erase(0x3D70, 0x240); // device cert
		erase(0x3FC0, 0x240); // device key

		writeHash(0x12E0, 0x0AE0, certSize());  // client cert hash
		writeCal0Hash();
		return verify();
	}

	bool import(const char* path)
	{
		FILE* f = fopen(path, "rb");

		if (!f)
		{
			printf("\x1b[31;1merror:\x1b[0m could not open %s\n", path);
			return false;
		}

		copy(f, 0x0200, 0x70); // serial
		copy(f, 0x0AD0, 0x04); // client size
		copy(f, 0x0AE0, 0x800); // client cert
		copy(f, 0x12E0, 0x20); // client cert hash
		copy(f, 0x3AE0, 0x130); // private key
		copy(f, 0x35E1, 0x006); // deviceId
		copy(f, 0x36E1, 0x006); // deviceId
		copy(f, 0x02B0, 0x180); // device cert
		copy(f, 0x3D70, 0x240); // device cert
		copy(f, 0x3FC0, 0x240); // device key

		fclose(f);
		return writeCal0Hash();
	}

	bool verify()
	{
		bool r = verifyHash(0x12E0, 0x0AE0, certSize()); // client cert hash
		r &= verifyHash(0x20, 0x0040, calibrationDataSize()); // calibration hash

		return r;
	}

	char* serial()
	{
		static char serialNumber[0x19];

		memset(serialNumber, 0, 0x18);

		if (fsStorageRead(&m_sh, 0x0250, serialNumber, 0x18))
		{
			printf("\x1b[31;1merror:\x1b[0m failed reading calibration data\n");
			sprintf(serialNumber, "%s-*", SwitchIdent_GetSerialNumber());
		}

		return serialNumber;
	}
	
	u32 calibrationDataSize()
	{
		return read<u32>(0x08);
	}
	
	u32 certSize()
	{
		return read<u32>(0x0AD0);
	}
	bool writeCal0Hash()
	{
		return writeHash(0x20, 0x0040, calibrationDataSize());
	}
	
	bool writeHash(u64 hashOffset, u64 offset, u64 sz)
	{
		u8* buffer = new u8[sz];

		if (fsStorageRead(&m_sh, offset, buffer, sz))
		{
			printf("\x1b[31;1merror:\x1b[0m failed reading calibration data\n");
		}
		else
		{
			u8 hash[0x20];

			sha256CalculateHash(hash, buffer, sz);

			if (fsStorageWrite(&m_sh, hashOffset, hash, sizeof(hash)))
			{
				printf("\x1b[31;1merror:\x1b[0m failed writing hash\n");
			if(isSpanish())
			printf("\x1b[36;1mEl CFW esta bloqueando el acceso al prodinfo\x1b[0m\n");
			else
			printf("\x1b[36;1mThe CFW is blocking the access to the prodinfo\x1b[0m\n");
			}
		}

		delete buffer;
		return true;
	}

	void print(u8* buffer, u64 sz)
	{
		for (u64 i = 0; i < sz; i++)
		{
			printf("%2.2X ", buffer[i]);
		}
		printf("\n");
	}

	bool verifyHash(u64 hashOffset, u64 offset, u64 sz)
	{
		bool result = false;
		u8* buffer = new u8[sz];

		if (fsStorageRead(&m_sh, offset, buffer, sz))
		{
			printf("\x1b[31;1merror:\x1b[0m failed reading calibration data\n");
		}
		else
		{
			u8 hash1[0x20];
			u8 hash2[0x20];

			sha256CalculateHash(hash1, buffer, sz);

			if (fsStorageRead(&m_sh, hashOffset, hash2, sizeof(hash2)))
			{
				printf("\x1b[31;1merror:\x1b[0m failed reading hash\n");
			}
			else
			{
				if (memcmp(hash1, hash2, sizeof(hash1)))
				{
					printf("\x1b[31;1merror:\x1b[0m hash verification failed for %x %d\n", (long)offset, (long)sz);
					print(hash1, 0x20);
					print(hash2, 0x20);
				}
				else
				{
					result = true;
				}
			}
		}

		delete buffer;
		return result;
	}

	template<class T>
	T read(u64 offset)
	{
		T buffer;

		if (fsStorageRead(&m_sh, offset, &buffer, sizeof(T)))
		{
			printf("\x1b[31;1merror:\x1b[0m failed reading %d bytes @ %x\n", (long)sizeof(T), (long)offset);
		}

		return buffer;
	}

	bool erase(u64 offset, u64 sz)
	{
		u8 zero = 0;

		for (u64 i = 0; i < sz; i++)
		{
			fsStorageWrite(&m_sh, offset + i, &zero, 1);
		}

		return true;
	}

	bool copy(FILE* f, u64 offset, u64 sz)
	{
		u8* buffer = new u8[size()];

		fseek(f, offset, 0);

		if (!fread(buffer, 1, sz, f))
		{
			printf("\x1b[31;1merror:\x1b[0m failed to read %d bytes from %x\n", (long)sz, (long)offset);
			return false;
		}

		fsStorageWrite(&m_sh, offset, buffer, sz);

		delete buffer;

		return true;
	}
	
protected:
	FsStorage m_sh;
	bool m_open;
};
bool Reboots()
{
	if(isSpanish()){
	printf("Pulsa + para reiniciar (recomendado)\n");	
	printf("Pulsa HOME para salir\n");
	}else{
	printf("Press + to Reboot(recomended)\n");	
	printf("Press HOME to exit\n");}
	appletEndBlockingHomeButton();
	while (appletMainLoop())
	{
		hidScanInput();

		u64 keys = hidKeysDown(CONTROLLER_P1_AUTO);

		if (keys & KEY_PLUS)
		{
		bpcInitialize();
		bpcRebootSystem();
		bpcExit();
		}
		consoleUpdate(NULL);
	}

	return true;
}
bool end()

{
	if(isSpanish())
	printf("Pulsa + para salir\n");
	else
	printf("Press + to exit\n");
	
	appletEndBlockingHomeButton();
	while (appletMainLoop())
	{
		hidScanInput();

		u64 keys = hidKeysDown(CONTROLLER_P1_AUTO);

		if (keys & KEY_PLUS)
		{
		break;		
		}
		consoleUpdate(NULL);
	}

	return true;
}

bool confirm()
{
	if(isSpanish()){
	printf("Seguro que quieres hacer esto?\n");
	printf("Pulse A para confirmar\n");
	}else{
	printf("Are you sure you want to do this?\n");
	printf("Press A to confirm\n");}

	while (appletMainLoop())
	{
		hidScanInput();

		u64 keys = hidKeysDown(CONTROLLER_P1_AUTO);

		if (keys & KEY_PLUS || keys & KEY_B || keys & KEY_X || keys & KEY_Y)
		{
			return false;
		}

		if (keys & KEY_A)
		{
			return true;
		}
		consoleUpdate(NULL);
	}
	return false;
}

bool install()
{
	if(isSpanish())
	printf("Estas seguro de que deseas borrar tu informacion personal del prodinfo?\n");
	else
	printf("Are you sure you want erase your personal information from prodinfo?\n");
	if (!confirm())
	{
		return end();
	}
	printf("Working...\n");
	consoleUpdate(NULL);
	Incognito incognito;
	incognito.clean();
	printf("new serial:       \x1b[32;1m%s\x1b[0m\n", incognito.serial());
	incognito.close();

	return Reboots();
}

bool verify()
{
/*	if (!fileExists("sdmc:/backup/prodinfo.bin"))
	{
	printf("\n\n");
	printf("\x1b[31;1merror: prodinfo.bin not found\n\n\x1b[0m");
	return false;
	}*/
	Incognito incognito;

	if (incognito.verify())
	{
		consoleUpdate(NULL);
		printf("\n\n");
		printf("\x1b[32;1mProdinfo verified\n\x1b[0m");

		return true;
	}
	else
	{
		consoleUpdate(NULL);
		printf("\x1b[31;1merror: Prodinfo is invalid\n\n\x1b[0m");
		
		return true;
	}
}

bool restore()
{
if(verify()){
	if (!confirm())
	{
		return end();
	}
	printf("Working...\n");
	consoleUpdate(NULL);
Incognito incognito;
	if (fileExists("sdmc:/backup/prodinfo.bin"))
	{
		if (!incognito.import("sdmc:/backup/prodinfo.bin"))
		{
			printf("\x1b[31;1merror:\x1b[0m failed to import prodinfo.bin\n");
			//return end();
		}
	}else{
	printf("\x1b[31;1m*:\x1b[0m /backup/prodinfo.bin does not exist on the SD card, do i restore it from the Nand? \n");
	consoleUpdate(NULL);
		//Ok prodifo.bin is not on SD
		FsFileSystem save2;
		fsOpenBisFileSystem(&save2, FsBisPartitionId_SafeMode, "");
        fsdevMountDevice("safemode2", save2);
		
		//first check if prodifo.bin is on nand
		if (fileExists("safemode2:/prodinfo.bin"))
		{
				if (!confirm())
				{
					fsdevUnmountDevice("safemode2");
					fsFsClose(&save2);
					return end();
				}
			//try to import from SAVE Partition
			if (!incognito.import("safemode2:/prodinfo.bin"))
			{
				printf("\x1b[31;1merror:\x1b[0m failed to import prodinfo.bin from nand\n");
				return end();
			}
		}else{
		printf("\x1b[31;1merror:\x1b[0m prodinfo.bin does not exist in the nand, Tas Bien Jodido \n");
		fsdevUnmountDevice("safemode2");
		fsFsClose(&save2);
		return end();
		}
	fsdevUnmountDevice("safemode2");
	fsFsClose(&save2);
	}
	printf("new serial:       \x1b[32;1m%s\x1b[0m\n", incognito.serial());
	incognito.close();

	if(isSpanish())
	printf("Finalizado, por favor reinicia\n");
	else
	printf("Finished, please Reboot\n");
	return Reboots();
	}
return mainMenu();
}

void printSerial()
{
	Incognito incognito;
	printf("\n");
	if(isSpanish())
	printf("\x1b[33;1m*\x1b[36;1m Numero de Serie:\x1b[32;1m%s\x1b[0m\n", incognito.serial());
	else
	printf("\x1b[33;1m*\x1b[36;1m Serial number:\x1b[32;1m%s\x1b[0m\n", incognito.serial());


	incognito.close();
}

bool mainMenu()
{

	while (appletMainLoop())
	{
		hidScanInput();

		u64 keys = hidKeysDown(CONTROLLER_P1_AUTO);
		
		if(strlen(SwitchIdent_GetSerialNumber()) != 0)
		{
			if (keys & KEY_A)
			{
				return install();
			}
		}
		
		if (keys & KEY_Y)
		{

			return restore();
		}

		if (keys & KEY_PLUS)
		{
			break;
		}		
		
		if(strlen(SwitchIdent_GetSerialNumber()) <= 0)
		{
			if (keys & KEY_MINUS)
			{
				return install();
			}
		}

		consoleUpdate(NULL);
	}
	return true;
}


int main(int argc, char **argv)
{
forceconfig();
	fsInitialize();
	consoleInit(NULL);
	appletBeginBlockingHomeButton(3);
	printf("\x1b[32;1m*\x1b[0m %s v%s Kronos2308 Mod \n",TITLE, VERSION);
	mkdir("/atmosphere/flags", 0700);
	printSerial();
				if(isSpanish())
				{
					printf("\x1b[31;1m*\x1b[0m Advertencia: Este software fue escrito por el Policia de la Scene.\n");
					printf("\x1b[31;1m*\x1b[0m Esta aplicacion corrompe intencionalmente el \x1b[31;1mPRODINFO\x1b[0m de la consola.\n");
					printf("\x1b[31;1m*\x1b[0m El objetivo es que la consola no conecta con Nintendo de ninguna forma.\n");
					printf("\x1b[31;1m*\x1b[0m Si se usa en Emunand solo afectara a Emunand.\n");
					printf("\x1b[31;1m*\x1b[0m Puede ser revertido en cualquier momento, Asi que no pierdas tu prodinfo.bin\n");
					printf("\x1b[31;1m*\x1b[0m Este software viene sin ninguna garantia.\n");
					printf("\x1b[31;1m*\x1b[0m No soy responsable de posibles fusiones Nucleares o Explosiones...\n");
					printf("\n\n\x1b[30;1m-------- Menu Principal --------\x1b[0m\n");
						if(strlen(SwitchIdent_GetSerialNumber()) != 0)
						printf("Pulsa A para Instalar incognito Mode\n");
						else
						printf("-----------------------------------\n* \x1b[30;1mIncognito esta Instalado\x1b[0m\n-----------------------------------\n");
					printf("Pulsa Y para Restaurar /backup/prodinfo.bin\n");
					printf("Pulsa + para Salir\n\n");
				}else{
					printf("\x1b[31;1m*\x1b[0m Warning: This software was written by a not nice person.\n");
					printf("\x1b[31;1m*\x1b[0m This application intentionally corrupts the console \x1b[31;1mProdinfo\x1b[0m partition.\n");
					printf("\x1b[31;1m*\x1b[0m The goal is that the console does not connect with Nintendo in any way.\n");
					printf("\x1b[31;1m*\x1b[0m If used in Emunand it will only affect Emunand.\n");
					printf("\x1b[31;1m*\x1b[0m It can be reversed at any time, So don't lose your prodinfo.bin.\n");
					printf("\x1b[31;1m*\x1b[0m this software come without any warranty.\n");
					printf("\x1b[31;1m*\x1b[0m I am not responsable of melt switch or nuclear explotions\n");
					printf("\n\n\x1b[30;1m-------- Main Menu --------\x1b[0m\n");
						if(strlen(SwitchIdent_GetSerialNumber()) != 0)
						printf("Press A to install incognito mode\n");
						else
						printf("\n-----------------------------------* \x1b[30;1mIncognito is Installed\x1b[0m\n\n-----------------------------------");
					printf("Press Y to restore /backup/prodinfo.bin\n");
					printf("Press + to exit\n\n");
				}
	mainMenu();
	appletEndBlockingHomeButton();
	consoleExit(NULL);
	fsExit();
	return 0;
}
