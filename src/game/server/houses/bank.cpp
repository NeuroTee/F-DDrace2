// made by fokkonaut

#include "bank.h"
#include <game/server/gamecontext.h>

CBank::CBank(CGameContext *pGameServer) : CHouse(pGameServer, HOUSE_BANK)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aAssignmentMode[i] = ASSIGNMENT_NONE;
		m_aCreditStep[i] = CREDIT_STEP_NONE;
		m_aCreditAmountPage[i] = AMOUNT_100;
		m_aCreditAmount[i] = 0;
	}
}

const char *CBank::GetWelcomeMessage(int ClientID)
{
	return Localizable("Welcome to the bank, %s! Press F4 to manage your bank account.");
}

const char *CBank::GetConfirmMessage(int ClientID)
{
	int Amount = GetAmount(m_aClients[ClientID].m_Page, ClientID);
	static char aBuf[128];
	if (m_aAssignmentMode[ClientID] == ASSIGNMENT_DEPOSIT)
	{
		str_format(aBuf, sizeof(aBuf), Localizable("Are you sure that you want to deposit %d money from your wallet to your bank account?"), Amount);
	}
	else if (m_aAssignmentMode[ClientID] == ASSIGNMENT_WITHDRAW)
	{
		str_format(aBuf, sizeof(aBuf), Localizable("Are you sure that you want to withdraw %d money from your bank account to your wallet?"), Amount);
	}
	else if (m_aAssignmentMode[ClientID] == ASSIGNMENT_CREDIT)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pPlayer->GetAccID()];
		if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM)
		{
			int TermDays = GetCreditTermDays(m_aClients[ClientID].m_Page);
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Take a credit of %lld money for %d days? Daily interest: %d%%."), m_aCreditAmount[ClientID], TermDays, GameServer()->Config()->m_SvBankCreditDailyInterest);
		}
		else if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
		{
			if (Amount > pAccount->m_CreditDebt)
				Amount = (int)pAccount->m_CreditDebt;
			str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Pay %d money towards your credit debt (%lld left)?"), Amount, pAccount->m_CreditDebt);
		}
	}
	return aBuf;
}

const char *CBank::GetEndMessage(int ClientID)
{
	if (m_aClients[ClientID].m_State == STATE_CHOSE_ASSIGNMENT)
		return Localizable("You can't use the bank without an account. Check '/account'.");
	return Localizable("You cancelled the assignment.");
}

bool CBank::NotLoggedIn(int ClientID)
{
	if (GameServer()->m_apPlayers[ClientID]->GetAccID() < ACC_START)
	{
		EndSession(ClientID, true);
		m_aAssignmentMode[ClientID] = ASSIGNMENT_NONE;
		return true;
	}
	return false;
}

void CBank::OnSuccess(int ClientID)
{
	if (NotLoggedIn(ClientID))
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pPlayer->GetAccID()];

	char aMsg[128];
	if (m_aAssignmentMode[ClientID] == ASSIGNMENT_DEPOSIT)
	{
		int Amount = GetAmount(m_aClients[ClientID].m_Page, ClientID);
		if (Amount <= 0)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You need to select an amount to deposit or withdraw."));
			return;
		}

		if (pPlayer->GetWalletMoney() < Amount)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money in your wallet to deposit this amount."));
			return;
		}

		pPlayer->BankTransaction(Amount, "deposit");
		pPlayer->WalletTransaction(-Amount, "deposit");

		str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You deposited %d money from your wallet to your bank account."), Amount);
		GameServer()->SendChatTarget(ClientID, aMsg);

	}
	else if (m_aAssignmentMode[ClientID] == ASSIGNMENT_WITHDRAW)
	{
		int Amount = GetAmount(m_aClients[ClientID].m_Page, ClientID);
		if (Amount <= 0)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You need to select an amount to deposit or withdraw."));
			return;
		}

		if (pAccount->m_Money < Amount)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money on your bank account to withdraw this amount."));
			return;
		}

		pPlayer->BankTransaction(-Amount, "withdraw");
		pPlayer->WalletTransaction(Amount, "withdraw");

		str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You withdrew %d money from your bank account to your wallet."), Amount);
		GameServer()->SendChatTarget(ClientID, aMsg);
	}
	else if (m_aAssignmentMode[ClientID] == ASSIGNMENT_CREDIT)
	{
		if (!GameServer()->Config()->m_SvBankCreditEnabled)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Credits are disabled."));
			return;
		}

		if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM)
		{
			if (pAccount->m_CreditDebt > 0)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You already have an active credit."));
				return;
			}

			int TermDays = GetCreditTermDays(m_aClients[ClientID].m_Page);
			int MinAmount = GameServer()->Config()->m_SvBankCreditMinAmount;
			int MaxAmount = GameServer()->Config()->m_SvBankCreditMaxAmount;
			int MinDays = GameServer()->Config()->m_SvBankCreditMinDays;
			int MaxDays = GameServer()->Config()->m_SvBankCreditMaxDays;
			if (m_aCreditAmount[ClientID] < MinAmount || m_aCreditAmount[ClientID] > MaxAmount || TermDays < MinDays || TermDays > MaxDays)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("Selected credit options are out of range."));
				return;
			}

			time_t Now;
			time(&Now);
			pAccount->m_CreditPrincipal = m_aCreditAmount[ClientID];
			pAccount->m_CreditDebt = m_aCreditAmount[ClientID];
			pAccount->m_CreditTermDays = TermDays;
			pAccount->m_CreditDaysLeft = TermDays;
			pAccount->m_CreditLastInterestDate = Now;

			pPlayer->BankTransaction(m_aCreditAmount[ClientID], "credit");
			GameServer()->WriteAccountStats(pPlayer->GetAccID());

			str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You received a credit of %lld money for %d days."), m_aCreditAmount[ClientID], TermDays);
			GameServer()->SendChatTarget(ClientID, aMsg);
		}
		else if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
		{
			int Amount = GetAmount(m_aClients[ClientID].m_Page, ClientID);
			if (Amount <= 0)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You need to select an amount to pay."));
				return;
			}

			if (pAccount->m_CreditDebt <= 0)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have an active credit."));
				return;
			}

			if (pAccount->m_Money < Amount)
			{
				GameServer()->SendChatTarget(ClientID, pPlayer->Localize("You don't have enough money in your bank account to pay this amount."));
				return;
			}

			if (Amount > pAccount->m_CreditDebt)
				Amount = (int)pAccount->m_CreditDebt;

			pPlayer->BankTransaction(-Amount, "credit payment");
			pAccount->m_CreditDebt -= Amount;

			if (pAccount->m_CreditDebt <= 0)
			{
				pAccount->m_CreditDebt = 0;
				pAccount->m_CreditPrincipal = 0;
				pAccount->m_CreditTermDays = 0;
				pAccount->m_CreditDaysLeft = 0;
				pAccount->m_CreditLastInterestDate = 0;
			}

			GameServer()->WriteAccountStats(pPlayer->GetAccID());

			str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You paid %d money towards your credit debt."), Amount);
			GameServer()->SendChatTarget(ClientID, aMsg);
		}
	}
}

void CBank::SetAssignment(int ClientID, int Dir)
{
	if (NotLoggedIn(ClientID))
		return;

	switch (Dir)
	{
	case -1:
		m_aAssignmentMode[ClientID] = ASSIGNMENT_WITHDRAW;
		m_aCreditStep[ClientID] = CREDIT_STEP_NONE;
		SetPage(ClientID, AMOUNT_100);
		break;
	case 1:
		m_aAssignmentMode[ClientID] = ASSIGNMENT_DEPOSIT;
		m_aCreditStep[ClientID] = CREDIT_STEP_NONE;
		SetPage(ClientID, AMOUNT_EVERYTHING);
		break;
	}
}

bool CBank::HandleKeyPress(int ClientID, int Dir)
{
	if (m_aAssignmentMode[ClientID] != ASSIGNMENT_CREDIT || m_aClients[ClientID].m_State != STATE_CHOSE_ASSIGNMENT)
		return false;

	if (m_aCreditStep[ClientID] == CREDIT_STEP_AMOUNT && Dir == 1)
	{
		int Amount = GetAmount(m_aClients[ClientID].m_Page, ClientID);
		if (Amount <= 0)
		{
			GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("You need to select a credit amount."));
			return true;
		}

		m_aCreditAmount[ClientID] = Amount;
		m_aCreditAmountPage[ClientID] = m_aClients[ClientID].m_Page;
		m_aCreditStep[ClientID] = CREDIT_STEP_TERM;
		int TermPage = GetFirstCreditTermPage();
		if (TermPage == PAGE_NONE)
		{
			GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("No valid credit term available."));
			m_aCreditStep[ClientID] = CREDIT_STEP_AMOUNT;
			return true;
		}
		SetPage(ClientID, TermPage);
		return true;
	}

	if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM && Dir == -1)
	{
		m_aCreditStep[ClientID] = CREDIT_STEP_AMOUNT;
		SetPage(ClientID, m_aCreditAmountPage[ClientID]);
		return true;
	}

	return false;
}

void CBank::OnMainPageChange(int ClientID, int Dir)
{
	(void)Dir;

	if (NotLoggedIn(ClientID))
		return;

	if (GameServer()->Config()->m_SvMoneyBankMode == 0)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("The bank is currently disabled."));
		return;
	}

	if (!GameServer()->Config()->m_SvBankCreditEnabled)
	{
		GameServer()->SendChatTarget(ClientID, GameServer()->m_apPlayers[ClientID]->Localize("Credits are disabled."));
		return;
	}

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pPlayer->GetAccID()];
	m_aAssignmentMode[ClientID] = ASSIGNMENT_CREDIT;
	if (pAccount->m_CreditDebt > 0)
	{
		m_aCreditStep[ClientID] = CREDIT_STEP_PAYMENT;
		int AmountPage = GetFirstCreditAmountPage(ClientID);
		if (AmountPage == PAGE_NONE)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("No valid credit payment amount available."));
			return;
		}
		SetPage(ClientID, AmountPage);
	}
	else
	{
		m_aCreditStep[ClientID] = CREDIT_STEP_AMOUNT;
		int AmountPage = GetFirstCreditAmountPage(ClientID);
		if (AmountPage == PAGE_NONE)
		{
			GameServer()->SendChatTarget(ClientID, pPlayer->Localize("No valid credit amount available."));
			return;
		}
		SetPage(ClientID, AmountPage);
	}
	m_aClients[ClientID].m_State = STATE_CHOSE_ASSIGNMENT;
}

void CBank::OnPageChange(int ClientID)
{
	char aMsg[512];
	const char *pFooter = "";
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if (m_aClients[ClientID].m_Page <= PAGE_MAIN)
	{
		m_aAssignmentMode[ClientID] = ASSIGNMENT_NONE;
		m_aCreditStep[ClientID] = CREDIT_STEP_NONE;
		if (GameServer()->Config()->m_SvMoneyBankMode == 0)
		{
			str_copy(aMsg, pPlayer->Localize("Welcome to the bank!\n\nThis feature is currently disabled and your money is instantly saved."), sizeof(aMsg));
		}
		else
		{
			if (GameServer()->Config()->m_SvBankCreditEnabled)
				str_copy(aMsg, pPlayer->Localize("Welcome to the bank!\n\nPlease select your option:\nF3: Deposit (+)\nF4: Withdraw (-)\nShoot: Credit.\n\nOnce you selected an option, shoot to the right to go one step forward, and shoot left to go one step back."), sizeof(aMsg));
			else
				str_copy(aMsg, pPlayer->Localize("Welcome to the bank!\n\nPlease select your option:\nF3: Deposit (+)\nF4: Withdraw (-).\n\nOnce you selected an option, shoot to the right to go one step forward, and shoot left to go one step back."), sizeof(aMsg));
		}
	}
	else
	{
		const char *pAssignment = m_aAssignmentMode[ClientID] == ASSIGNMENT_DEPOSIT ? pPlayer->Localize("D E P O S I T") :
			m_aAssignmentMode[ClientID] == ASSIGNMENT_WITHDRAW ? pPlayer->Localize("W I T H D R A W") :
			m_aAssignmentMode[ClientID] == ASSIGNMENT_CREDIT ? pPlayer->Localize("C R E D I T") : "";

		if (m_aAssignmentMode[ClientID] == ASSIGNMENT_CREDIT)
		{
			CGameContext::AccountInfo *pAccount = &GameServer()->m_Accounts[pPlayer->GetAccID()];
			str_format(aMsg, sizeof(aMsg), "%s: %lld\n%s: %lld\n%s: %lld\n%s: %d\n\n%s\n\n", pPlayer->Localize("Bank"), pAccount->m_Money,
				pPlayer->Localize("Wallet"), pPlayer->GetWalletMoney(), pPlayer->Localize("Credit debt"), pAccount->m_CreditDebt,
				pPlayer->Localize("Days left"), pAccount->m_CreditDaysLeft, pAssignment);

			char aSelection[64];
			if (m_aCreditStep[ClientID] == CREDIT_STEP_AMOUNT)
			{
				pFooter = pPlayer->Localize("Press F3 to select the term.");
				int Type = m_aClients[ClientID].m_Page;
				str_format(aSelection, sizeof(aSelection), "%d", GetAmount(Type));
			}
			else if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM)
			{
				pFooter = pPlayer->Localize("Press F3 to confirm the credit. F4 goes back.");
				int TermDays = GetCreditTermDays(m_aClients[ClientID].m_Page);
				str_format(aSelection, sizeof(aSelection), pPlayer->Localize("%d days"), TermDays);
			}
			else if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
			{
				pFooter = pPlayer->Localize("Press F3 to confirm the payment.");
				int Type = m_aClients[ClientID].m_Page;
				str_format(aSelection, sizeof(aSelection), "%d", GetAmount(Type));
			}

			char aBuf[96];
			if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM)
				str_format(aBuf, sizeof(aBuf), "%s: %lld\n- > %s < +", pPlayer->Localize("Amount"), m_aCreditAmount[ClientID], aSelection);
			else
				str_format(aBuf, sizeof(aBuf), "- > %s < +", aSelection);
			str_append(aMsg, aBuf, sizeof(aMsg));
		}
		else
		{
			pFooter = pPlayer->Localize("Press F3 to confirm your assignment.");
			str_format(aMsg, sizeof(aMsg), "%s: %lld\n%s: %lld\n\n%s\n\n", pPlayer->Localize("Bank"), GameServer()->m_Accounts[pPlayer->GetAccID()].m_Money,
				pPlayer->Localize("Wallet"), pPlayer->GetWalletMoney(), pAssignment);

			char aAmount[64];
			int Type = m_aClients[ClientID].m_Page;
			if (Type == AMOUNT_EVERYTHING)
				str_format(aAmount, sizeof(aAmount), pPlayer->Localize("Everything (%d)"), GetAmount(Type, ClientID));
			else
				str_format(aAmount, sizeof(aAmount), "%d", GetAmount(Type));

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "- > %s < +", aAmount);
			str_append(aMsg, aBuf, sizeof(aMsg));
		}
	}

	SendWindow(ClientID, aMsg, pFooter);
	m_aClients[ClientID].m_LastMotd = Server()->Tick();
}

bool CBank::PageValid(int ClientID, int Page)
{
	if (m_aAssignmentMode[ClientID] == ASSIGNMENT_CREDIT)
	{
		if (m_aCreditStep[ClientID] == CREDIT_STEP_TERM)
		{
			if (!IsTermPage(Page))
				return false;
			int TermDays = GetCreditTermDays(Page);
			return TermDays >= GameServer()->Config()->m_SvBankCreditMinDays && TermDays <= GameServer()->Config()->m_SvBankCreditMaxDays;
		}

		if (!IsAmountPage(Page))
			return false;

		if (Page == AMOUNT_EVERYTHING)
			return false;

		int Amount = GetAmount(Page, ClientID);
		if (Amount <= 0)
			return false;

		if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
		{
			int64 Debt = GameServer()->m_Accounts[GameServer()->m_apPlayers[ClientID]->GetAccID()].m_CreditDebt;
			if (Debt <= 0)
				return false;
			if (Debt < GetAmount(AMOUNT_100))
				return Page == AMOUNT_100;
			return Amount <= Debt;
		}

		return Amount >= GameServer()->Config()->m_SvBankCreditMinAmount && Amount <= GameServer()->Config()->m_SvBankCreditMaxAmount;
	}

	return IsAmountPage(Page);
}

int CBank::GetAmount(int Type, int ClientID)
{
	if (Type == AMOUNT_EVERYTHING)
	{
		if (ClientID >= 0)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
			if (!pPlayer)
				return 0;

			if (m_aAssignmentMode[ClientID] == ASSIGNMENT_DEPOSIT)
				return pPlayer->GetWalletMoney();
			else if (m_aAssignmentMode[ClientID] == ASSIGNMENT_WITHDRAW)
				return GameServer()->m_Accounts[pPlayer->GetAccID()].m_Money;
		}
		return 0;
	}

	return GetFixedAmount(Type);
}

int CBank::GetFixedAmount(int Type)
{
	switch (Type)
	{
	case AMOUNT_100: return 100;
	case AMOUNT_1K: return 1000;
	case AMOUNT_5K: return 5000;
	case AMOUNT_10K: return 10000;
	case AMOUNT_50K: return 50000;
	case AMOUNT_100K: return 100000;
	case AMOUNT_500K: return 500000;
	case AMOUNT_1MIL: return 1000000;
	case AMOUNT_5MIL: return 5000000;
	case AMOUNT_10MIL: return 10000000;
	case AMOUNT_50MIL: return 50000000;
	case AMOUNT_100MIL: return 100000000;
	default: return 0;
	}
}

int CBank::GetCreditTermDays(int Type) const
{
	switch (Type)
	{
	case TERM_1D: return 1;
	case TERM_3D: return 3;
	case TERM_7D: return 7;
	case TERM_14D: return 14;
	case TERM_30D: return 30;
	default: return 0;
	}
}

bool CBank::IsAmountPage(int Page) const
{
	return Page >= AMOUNT_EVERYTHING && Page <= AMOUNT_100MIL;
}

bool CBank::IsTermPage(int Page) const
{
	return Page >= TERM_1D && Page <= TERM_30D;
}

int CBank::GetFirstCreditAmountPage(int ClientID) const
{
	int MinAmount = GameServer()->Config()->m_SvBankCreditMinAmount;
	int MaxAmount = GameServer()->Config()->m_SvBankCreditMaxAmount;
	int64 CreditDebt = 0;
	if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
		CreditDebt = GameServer()->m_Accounts[GameServer()->m_apPlayers[ClientID]->GetAccID()].m_CreditDebt;

	if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT && CreditDebt > 0 && CreditDebt < GetAmount(AMOUNT_100))
		return AMOUNT_100;

	for (int Page = AMOUNT_100; Page <= AMOUNT_100MIL; Page++)
	{
		int Amount = GetAmount(Page, ClientID);
		if (Amount <= 0)
			continue;

		if (m_aCreditStep[ClientID] == CREDIT_STEP_PAYMENT)
		{
			if (Amount <= CreditDebt)
				return Page;
		}
		else if (Amount >= MinAmount && Amount <= MaxAmount)
		{
			return Page;
		}
	}

	return PAGE_NONE;
}

int CBank::GetFirstCreditTermPage() const
{
	int MinDays = GameServer()->Config()->m_SvBankCreditMinDays;
	int MaxDays = GameServer()->Config()->m_SvBankCreditMaxDays;
	for (int Page = TERM_1D; Page <= TERM_30D; Page++)
	{
		int TermDays = GetCreditTermDays(Page);
		if (TermDays >= MinDays && TermDays <= MaxDays)
			return Page;
	}

	return PAGE_NONE;
}
