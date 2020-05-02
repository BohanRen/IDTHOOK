/*
	IDT HOOK ----By:kanren
	PS:�������Լ�д��ֻ��д�����
	ע������:
	1.IDTR��IDTENTRY�Ľṹ�壬WRK����д��x86��x64��ͬ����ע��ṹ���ֽڶ��� ��ϸԭ�����е���
	2.IDTENTRY����ֻ��ҳ�棬��Cr0 WPλ��0�ſ���д��
	3.IDTENTRY�еĳ�Ա˵��:OffsetHigh(��ַ�ĸ�32λ), OffsetMiddle(��ַ��32λ�ĸ�16λ), OffsetLow(��ַ�ĵ�16λ)
	4.sidt��ȡ��ǰCPU���ĵ�IDTR,��˴�������Ѿ���������
	5.IDT����PG������Χ�����밲ȫʹ�ã�����׼��PassPG
	6.���뻷��:Win10 10.0.15063 SDK WDK10.0 VS2015
	Դ������ο����������⣬��������
*/

#include <ntifs.h>
#include <windef.h>
#include "Tools.h"

#define oprintf(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)

IDTR idtr;
ULONG_PTR OldTrap0E = 0;
ULONG_PTR Trap0E_RET = 0;
VOID DriverUnload(IN PDRIVER_OBJECT pDriverObj)
{
	DbgPrint("DriverUnload\n");
}

KIRQL WPOFFx64()
{
	KIRQL  irql = KeRaiseIrqlToDpcLevel();
	UINT64  cr0 = __readcr0();
	cr0 &= 0xfffffffffffeffff;
	__writecr0(cr0);
	_disable();
	return  irql;
}

void WPONx64(KIRQL irql)
{
	UINT64 cr0 = __readcr0();
	cr0 |= 0x10000;
	_enable();
	__writecr0(cr0);
	KeLowerIrql(irql);
}



ULONG64 GetIdtAddr(ULONG64 IdtBaseAddr, UCHAR Index)
{
	PIDT_ENTRY Pidt = (PIDT_ENTRY)(IdtBaseAddr);
	Pidt = Pidt + Index;
	ULONG64 OffsetHigh, OffsetMiddle, OffsetLow, ret;

	OffsetHigh = Pidt->idt.OffsetHigh;
	OffsetHigh = OffsetHigh << 32;

	OffsetMiddle = Pidt->idt.OffsetMiddle;
	OffsetMiddle = OffsetMiddle << 16;



	OffsetLow = Pidt->idt.OffsetLow;
	ret = OffsetHigh + OffsetMiddle + OffsetLow;
	return ret;
}

ULONG64 SetIdtAddr(ULONG64 IdtBaseAddr, UCHAR Index, ULONG64 NewAddr)
{
	PIDT_ENTRY Pidt = (PIDT_ENTRY)(IdtBaseAddr);
	Pidt = Pidt + Index;
	ULONG64 OffsetHigh, OffsetMiddle, OffsetLow, ret;


	OffsetHigh = Pidt->idt.OffsetHigh;
	OffsetHigh = OffsetHigh << 32;
	OffsetMiddle = Pidt->idt.OffsetMiddle;
	OffsetMiddle = OffsetMiddle << 16;

	OffsetLow = Pidt->idt.OffsetLow;
	ret = OffsetHigh + OffsetMiddle + OffsetLow;
	Pidt->idt.OffsetHigh = NewAddr >> 32;
	Pidt->idt.OffsetMiddle = NewAddr << 32 >> 48;
	Pidt->idt.OffsetLow = NewAddr & 0xFFFF;
	return ret;
}

VOID HookJmp(UINT64 Addr, PVOID JmpAddr){

	*(UINT16*)Addr = 0xFEEB;//jmp self
	*(UINT32*)(Addr + 2) = 0;
	*(PVOID*)(Addr + 6) = JmpAddr;
	*(UINT16*)Addr = 0x25FF;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObj, IN PUNICODE_STRING pRegistryString)
{	
	oprintf("DriverEntry\n");
	pDriverObj->DriverUnload = DriverUnload;

	__sidt(&idtr);
	oprintf("IDT Base:%llX\n", idtr.Base);
	oprintf("Old IDT[0xE]:%llX\n", GetIdtAddr(idtr.Base, 0xE));

	OldTrap0E = GetIdtAddr(idtr.Base, 0xE);

	

	//inline hook 0E 

	/*Trap0E_RET = OldTrap0E + 0x10;
	KIRQL IRQL = WPOFFx64();
	HookJmp(OldTrap0E, Trap0E_Ori);
	WPONx64(IRQL);*/
	


	for (int i = 0; i < KeNumberProcessors; i++){//IDT HOOK���shark��Լ��12-24Сʱ�����ջ������ײͣ����Ի��������win10 1709

		KeSetSystemAffinityThread((KAFFINITY)(1 << i));
		__sidt(&idtr);
		oprintf("CPU[%d] IDT Base:%llX\n", i, idtr.Base);
		KIRQL IRQL = WPOFFx64();
		//oprintf("CPU[%d] Old IDT[0xEA]:%llX\n", i, GetIdtAddr(idtr.Base, 0xE));

		oprintf("CPU[%d] Old IDT[0xE]:%llX\n", i, SetIdtAddr(idtr.Base, 0xE, (ULONG64)HookTrap0E));
		WPONx64(IRQL);
		KeRevertToUserAffinityThread();
	}

	return STATUS_SUCCESS;
}