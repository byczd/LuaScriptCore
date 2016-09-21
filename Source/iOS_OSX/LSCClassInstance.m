//
//  LSCClassInstance.m
//  LuaScriptCore
//
//  Created by 冯鸿杰 on 16/9/19.
//  Copyright © 2016年 vimfung. All rights reserved.
//

#import "LSCClassInstance.h"
#import "LSCClassInstance_Private.h"
#import "LSCValue_Private.h"

@implementation LSCClassInstance

- (instancetype)initWithState:(lua_State *)state atIndex:(int)index
{
    if (self = [super init])
    {
        self.state = state;
        
        int top = lua_gettop(state);
        if (top < index)
        {
            return nil;
        }
        
        if (lua_istable(state, index))
        {
            lua_pushvalue(state, index);
            
            lua_pushstring(state, "_nativeClassName");
            lua_gettable(state, -2);
            if (lua_isstring(state, -1))
            {
                NSString *className = [NSString stringWithUTF8String:lua_tostring(state, -1)];
                self.ownerClass = NSClassFromString(className);
            }
            else
            {
                lua_settop(state, -3);
                return nil;
            }
            
            lua_settop(state, -2);
            
            lua_pushstring(state, "_nativeObject");
            lua_gettable(state, -2);
            if (lua_islightuserdata(state, -1))
            {
                self.nativeObject = (__bridge LSCObjectClass *)(lua_topointer(state, -1));
            }
            else
            {
                lua_settop(state, -3);
                return nil;
            }
            
            lua_settop(state, -3);
        }
        else
        {
            return nil;
        }
        
    }
    return self;
}

- (void)setField:(LSCValue *)value forName:(NSString *)name
{
    [value pushWithState:self.state];
    lua_setfield(self.state, -2, [name UTF8String]);
}

- (LSCValue *)getFieldForName:(NSString *)name
{
    lua_pushstring(self.state, [name UTF8String]);
    lua_gettable(self.state, -2);
    
    return [LSCValue valueWithState:self.state atIndex:-1];
}

@end