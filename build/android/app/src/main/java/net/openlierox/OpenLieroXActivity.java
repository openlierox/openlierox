package net.openlierox;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class OpenLieroXActivity extends SDLActivity {

    private static final String TAG = "OpenLieroX";

    @Override
    protected String[] getLibraries() {
        // Order matters: SDL first, then transitive deps, then the
        // game library named "main" (libmain.so) which contains
        // SDL_main and is what SDLActivity invokes.
        return new String[] {
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "main"
        };
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Extract the game data shipped under assets/gamedir into
        // app-private storage before the native side starts using it.
        try {
            extractGamedirIfNeeded();
        } catch (IOException e) {
            Log.e(TAG, "Failed to extract gamedir: " + e.getMessage(), e);
        }
        super.onCreate(savedInstanceState);
    }

    private void extractGamedirIfNeeded() throws IOException {
        AssetManager am = getAssets();
        File filesDir = getFilesDir();
        File targetRoot = new File(filesDir, "OpenLieroX");
        File versionFile = new File(targetRoot, ".gamedir.version");

        String packagedVersion = readAssetText(am, "gamedir.version");
        String installedVersion = readFileText(versionFile);

        if (installedVersion != null && installedVersion.equals(packagedVersion)) {
            Log.i(TAG, "Game data already extracted (version " + installedVersion + ")");
            return;
        }

        Log.i(TAG, "Extracting game data (packaged " + packagedVersion +
                  ", installed " + installedVersion + ") to " + targetRoot);
        targetRoot.mkdirs();
        copyAssetTree(am, "gamedir", targetRoot);

        try (FileOutputStream out = new FileOutputStream(versionFile)) {
            out.write(packagedVersion.getBytes("UTF-8"));
        }
    }

    private void copyAssetTree(AssetManager am, String assetPath, File targetDir) throws IOException {
        String[] entries = am.list(assetPath);
        if (entries == null || entries.length == 0) {
            // Leaf — copy as a regular file.
            copyAssetFile(am, assetPath, new File(targetDir.getParentFile(), targetDir.getName()));
            return;
        }
        targetDir.mkdirs();
        for (String entry : entries) {
            String childAssetPath = assetPath + "/" + entry;
            File childTarget = new File(targetDir, entry);
            String[] grandEntries = am.list(childAssetPath);
            if (grandEntries != null && grandEntries.length > 0) {
                copyAssetTree(am, childAssetPath, childTarget);
            } else {
                copyAssetFile(am, childAssetPath, childTarget);
            }
        }
    }

    private void copyAssetFile(AssetManager am, String assetPath, File outFile) throws IOException {
        outFile.getParentFile().mkdirs();
        try (InputStream in = am.open(assetPath);
             OutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        }
    }

    private String readAssetText(AssetManager am, String assetPath) {
        try (InputStream in = am.open(assetPath)) {
            byte[] buf = new byte[256];
            int n = in.read(buf);
            if (n <= 0) return "";
            return new String(buf, 0, n, "UTF-8").trim();
        } catch (IOException e) {
            return "";
        }
    }

    private String readFileText(File f) {
        if (!f.isFile()) return null;
        try (java.io.FileInputStream in = new java.io.FileInputStream(f)) {
            byte[] buf = new byte[256];
            int n = in.read(buf);
            if (n <= 0) return "";
            return new String(buf, 0, n, "UTF-8").trim();
        } catch (IOException e) {
            return null;
        }
    }
}
