//
//  CLabel.cpp
//  OpenLieroX
//
//  Created by Albert Zeyer on 09.01.12.
//  code under LGPL
//

#include "DeprecatedGUI/CLabel.h"
#include "LieroX.h"

namespace DeprecatedGUI {

	void	CLabel::Create() 
	{ 
		iWidth = tLX->cFont.GetWidth(sText); 
		iHeight = tLX->cFont.GetHeight(sText);
		if( bCenter ) 
			iX -= iWidth / 2;
	}
		
	uintptr_t CLabel::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)	{ return 0; }
	uintptr_t CLabel::SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { 
		if (iMsg == LBS_SETTEXT) 
		{sText = sStr; iWidth = tLX->cFont.GetWidth(sText); iHeight = tLX->cFont.GetHeight(sText);} 
		return 0; 	}
	
	uintptr_t CLabel::SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }
	
	void	CLabel::ChangeColour(Color col)			{ iColour = col; }
	void	CLabel::setText(const std::string& sStr)	{sText = sStr; iWidth = tLX->cFont.GetWidth(sText); iHeight = tLX->cFont.GetHeight(sText);};
	std::string CLabel::getText() const { return sText; }
	
	// Draw the label
	void CLabel::Draw(SDL_Surface * bmpDest) {
		if (bRedrawMenu)
			redrawBuffer();
		if( bVar )
			if( *bVar )
				sText = "yes";
			else
				sText = "no";
			else if( iVar )
				sText = itoa( *iVar );
			else if( fVar )
				sText = ftoa( *fVar );
			else if( sVar )
				sText = *sVar;
		if( bCenter )
		{
			int width = tLX->cFont.GetWidth(sText);
			iX += ( iWidth - width ) / 2;
			iWidth = width;
		}
		tLX->cFont.Draw(bmpDest, iX, iY, iColour,sText); 
	}

	CWidget * CLabel::WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
	{
		CLabel * w = new CLabel( p[0].toString(), p[1].toColor(), p[2].toBool() );
		w->bVar = CScriptableVars::GetVarP<bool>( p[3].toString() );
		w->iVar = CScriptableVars::GetVarP<int>( p[3].toString() );
		w->fVar = CScriptableVars::GetVarP<float>( p[3].toString() );
		w->sVar = CScriptableVars::GetVarP<std::string>( p[3].toString() );
		layout->Add( w, id, x, y, dx, dy );
		return w;
	}

};
