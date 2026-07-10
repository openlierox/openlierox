function getKey(k)
	return settings.GameOptions.Ply1Controls[k]
end

function bindings.gamePrepare()

	message("Kill that bot 10 times, and avoid his shots.\n\n"
		.. "An essential skill to do this is 'comboing': "
		.. "hold your weapon selection key (" .. getKey("SelectWeapon") .. "),\n"
		.. "then press Shoot key (" .. getKey("Shoot") .. ") and "
		.. "quickly press Left or Right keys four times (" .. getKey("Left") .. "/" .. getKey("Right") .. ").\n"
		.. "This will result in a deadly burst of five shots!\n"
		.. "Move or aim up/down while shooting, and you'll cover larger area with shots."
		)

end
