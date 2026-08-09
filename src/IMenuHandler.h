#pragma once

class IMenuHandler {
public:
    virtual ~IMenuHandler() = default;
    virtual void onOpen() = 0;
    virtual void onReload() = 0;
    virtual void onClose() = 0;
    virtual void onReset() = 0;
    virtual void onQuit() = 0;
    virtual void onSaveState(int slot) = 0;
    virtual void onLoadState(int slot) = 0;
    virtual bool isMenuVisible() = 0;
};