// made by fokkonaut

#ifndef GAME_HOUSES_BANK_H
#define GAME_HOUSES_BANK_H

#include "house.h"
#include <engine/shared/protocol.h>

enum BankPages
{
	AMOUNT_EVERYTHING = PAGE_MAIN+1,
	AMOUNT_100,
	AMOUNT_1K,
	AMOUNT_5K,
	AMOUNT_10K,
	AMOUNT_50K,
	AMOUNT_100K,
	AMOUNT_500K,
	AMOUNT_1MIL,
	AMOUNT_5MIL,
	AMOUNT_10MIL,
	AMOUNT_50MIL,
	AMOUNT_100MIL,
	TERM_1D,
	TERM_3D,
	TERM_7D,
	TERM_14D,
	TERM_30D,
	NUM_PAGES_BANK
};

enum BankAssignment
{
	ASSIGNMENT_NONE,
	ASSIGNMENT_DEPOSIT,
	ASSIGNMENT_WITHDRAW,
	ASSIGNMENT_CREDIT,
};

enum CreditStep
{
	CREDIT_STEP_NONE,
	CREDIT_STEP_AMOUNT,
	CREDIT_STEP_TERM,
	CREDIT_STEP_PAYMENT,
};

class CBank : public CHouse
{
private:
	int GetAmount(int Type, int ClientID = -1);
	int GetCreditTermDays(int Type);
	bool IsAmountPage(int Page);
	bool IsTermPage(int Page);
	int GetFirstCreditAmountPage(int ClientID);
	int GetFirstCreditTermPage();
	int m_aAssignmentMode[MAX_CLIENTS];
	int m_aCreditStep[MAX_CLIENTS];
	int m_aCreditAmountPage[MAX_CLIENTS];
	int64 m_aCreditAmount[MAX_CLIENTS];
	bool NotLoggedIn(int ClientID);

	virtual int FirstPage() { return AMOUNT_EVERYTHING; }
	virtual int NumPages() { return NUM_PAGES_BANK; }
	virtual bool PageValid(int ClientID, int Page);
	virtual bool HandleKeyPress(int ClientID, int Dir);
	virtual void OnMainPageChange(int ClientID, int Dir);

public:
	CBank(CGameContext *pGameServer);
	virtual ~CBank() {};

	virtual void OnPageChange(int ClientID);
	virtual void OnSuccess(int ClientID);
	virtual const char *GetWelcomeMessage(int ClientID);
	virtual const char *GetConfirmMessage(int ClientID);
	virtual const char *GetEndMessage(int ClientID);
	virtual void SetAssignment(int ClientID, int Dir);
};

#endif // GAME_HOUSES_BANK_H
