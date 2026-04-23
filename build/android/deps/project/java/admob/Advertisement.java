/*
Simple DirectMedia Layer
Java source code (C) 2009-2014 Sergii Pylypenko

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required. 
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

package net.sourceforge.clonekeenplus;

import android.app.Activity;
import android.content.Context;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;
import android.view.View;

import com.google.android.gms.ads.*; // Copy google-play-services.jar from Google SDK to the directory project/libs, and update it's version number in AndroidManifest.xml

class Advertisement
{
	private AdView ad;
	MainActivity parent;

	public Advertisement(MainActivity p)
	{
		parent = p;
		AdSize adSize = AdSize.BANNER;

		if( Globals.AdmobBannerSize.equals("BANNER") )
			adSize = AdSize.BANNER;
		else if( Globals.AdmobBannerSize.equals("LEADERBOARD") )
			adSize = AdSize.LEADERBOARD;
		else if( Globals.AdmobBannerSize.equals("FULL_BANNER") )
			adSize = AdSize.FULL_BANNER;
		else if( Globals.AdmobBannerSize.equals("MEDIUM_RECTANGLE") )
			adSize = AdSize.MEDIUM_RECTANGLE;
		else if( Globals.AdmobBannerSize.equals("SMART_BANNER") )
			adSize = AdSize.SMART_BANNER;
		else if( Globals.AdmobBannerSize.equals("WIDE_SKYSCRAPER") )
			adSize = AdSize.WIDE_SKYSCRAPER;
		else
		{
			String[] size = Globals.AdmobBannerSize.split(":");
			int width = size[0].equals("FULL_WIDTH") ? AdSize.FULL_WIDTH : Integer.parseInt(size[0]);
			int height = size[1].equals("AUTO_HEIGHT") ? AdSize.AUTO_HEIGHT : Integer.parseInt(size[1]);
			adSize = new AdSize(width, height);
		}
		ad = new AdView(parent);
		ad.setAdSize(adSize);
		ad.setAdUnitId(Globals.AdmobPublisherId);
		AdRequest adRequest = new AdRequest.Builder().
			addTestDevice(AdRequest.DEVICE_ID_EMULATOR).
			addTestDevice(Globals.AdmobTestDeviceId).
			build();
		ad.loadAd(adRequest);
	}

	public View getView()
	{
		return ad;
	}

	public void requestNewAd()
	{
		AdRequest adRequest = new AdRequest.Builder().
			addTestDevice(AdRequest.DEVICE_ID_EMULATOR).
			addTestDevice(Globals.AdmobTestDeviceId).
			build();
		ad.loadAd(adRequest);
	}
}
