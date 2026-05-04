/*************************************************************************************************
*
* File: MIDI.h
*
* Description: Defines the MIDI objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include <vector>
#include <queue>
#include <string>
using namespace std;

#include "Misc.h"

//Classes defined in this file
class MIDI;
class MIDITrack;
class MIDIEvent;
class MIDIChannelEvent;
class MIDIMetaEvent;
class MIDISysExEvent;
class MIDIPos;

class MIDIDevice;
class MIDIOutDevice;

//
// MIDI File Classes
//

class MIDIPos
{
public:
    MIDIPos( MIDI &midi );

    bool GetNextEvent( MIDIEvent **pEvent );

    bool IsStandard() const { return m_bIsStandard; }
    int GetTicksPerBeat() const { return m_iTicksPerBeat; }
    int GetTicksPerSecond() const { return m_iTicksPerSecond; }
    int GetMicroSecsPerBeat() const { return m_iMicroSecsPerBeat; }

private:
	struct TrackInfo
	{
		unsigned long TrackID;
		int AbsT;
		mutable size_t EventIndex;

		// Wrong order on purpose, stupid priority_queue
		bool operator< (const TrackInfo& other) const
		{
			int diff2 = other.AbsT - AbsT;
			if(diff2)
				return diff2 < 0;

			long diff1 = other.TrackID - TrackID;
			if(diff1)
				return diff1 < 0;

			return false;
		}
	};

    // Where are we in the file?
    MIDI &m_MIDI;
    priority_queue<TrackInfo> m_vTrackPos;

    // Tempo variables
    bool m_bIsStandard;
    int m_iTicksPerBeat, m_iMicroSecsPerBeat; // For standard division
    int m_iTicksPerSecond; // For SMPTE division
};

//Holds MIDI data
class MIDI
{
public:
	static void InitArrays();

    enum Note { A, AS, B, C, CS, D, DS, E, F, FS, G, GS };

    static const int KEYS = 129; // One extra because 128th is a sharp
    static const int C8 = 108;
    static const int C4 = C8 - 4 * 12;
    static const int A0 = C8 - 7 * 12 - 3;
    static const int Drums = 0x09;
    static const wstring Instruments[129];
    static const wstring &NoteName( int iNote );
    static Note NoteVal( int iNote );
    static bool IsSharp( int iNote );
    static int WhiteCount( int iMinNote, int iMaxNote );

    //Generally usefull static parsing functions
    static int ParseVarNum( const unsigned char *pcData, int iMaxSize, int *piOut );
    static int Parse32Bit( const unsigned char *pcData, int iMaxSize, int *piOut );
    static int Parse24Bit( const unsigned char *pcData, int iMaxSize, int *piOut );
    static int Parse16Bit( const unsigned char *pcData, int iMaxSize, int *piOut );
    static int ParseNChars( const unsigned char *pcData, int iNChars, int iMaxSize, char *pcOut );

    MIDI( void ) {};
    MIDI( const wstring &sFilename );
    ~MIDI( void );

    //Parsing functions that load data into the instance
    int ParseMIDI( const unsigned char *pcData, int iMaxSize );
    int ParseTracks( const unsigned char *pcData, int iMaxSize );
    int ParseEvents( const unsigned char *pcData, int iMaxSize );
    bool IsValid() const { return ( m_vTracks.size() > 0 && m_Info.iNoteCount > 0 && m_Info.iDivision > 0 ); }

    void PostProcess() { PostProcess( NULL ); } 
    void PostProcess( vector< MIDIEvent* > *vEvents );
    void ConnectNotes();
    void clear( void );

    friend class MIDIPos;

    struct MIDIInfo
    {
        MIDIInfo() { clear(); }
        void clear() { llTotalMicroSecs = llFirstNote = iFormatType = iNumTracks = iNumChannels = iDivision = iMinNote =
                       iMaxNote = iNoteCount = iEventCount = iMaxVolume = iVolumeSum = iTotalTicks = iTotalBeats = 0;
                       sFilename.clear(); }
        void AddTrackInfo( const MIDITrack &mTrack);

        wstring sFilename;
        int iFormatType;
        int iNumTracks, iNumChannels;
        int iDivision;
        int iMinNote, iMaxNote, iNoteCount, iEventCount;
        int iMaxVolume, iVolumeSum;
        int iTotalTicks, iTotalBeats;
        long long llTotalMicroSecs, llFirstNote;
    };

    const MIDIInfo& GetInfo() const { return m_Info; }
    const vector< MIDITrack* >& GetTracks() const { return m_vTracks; }

private:
    static wstring aNoteNames[KEYS + 1];
    static Note aNoteVal[KEYS];
    static bool aIsSharp[KEYS];
    static int aWhiteCount[KEYS + 1];

    MIDIInfo m_Info;
    vector< MIDITrack* > m_vTracks;
};

//Holds all the event of one MIDI track
class MIDITrack
{
public:
    ~MIDITrack( void );

    //Parsing functions that load data into the instance
    int ParseTrack( const unsigned char *pcData, int iMaxSize, int iTrack );
    int ParseEvents( const unsigned char *pcData, int iMaxSize, int iTrack );
    void clear( void );

    friend class MIDIPos;
    friend class MIDI;

    struct MIDITrackInfo
    {
        MIDITrackInfo() { clear(); }
        void clear() { llTotalMicroSecs = iSequenceNumber = iMinNote = iMaxNote = iNoteCount = 
                       iEventCount = iMaxVolume = iVolumeSum = iTotalTicks = iNumChannels = 0;
                       memset( aNoteCount, 0, sizeof( aNoteCount ) ),
                       memset( aProgram, 0, sizeof( aProgram ) ),
                       sSequenceName.clear(); }
        void AddEventInfo( const MIDIEvent &mTrack );

        int iSequenceNumber;
        string sSequenceName;
        int iMinNote, iMaxNote, iNoteCount, iEventCount;
        int iMaxVolume, iVolumeSum;
        int iTotalTicks;
        long long llTotalMicroSecs;
        int aNoteCount[16], aProgram[16], iNumChannels;
    };
    const MIDITrackInfo& GetInfo() const { return m_TrackInfo; }

private:
    MIDITrackInfo m_TrackInfo;
    vector< MIDIEvent* > m_vEvents;
};

//Base Event class
//Should really be a single class with unions for the different events. much faster that way.
//Might be forced to convert if batch processing is too slow
class MIDIEvent
{
public:
    //Event types
    enum EventType : unsigned char { ChannelEvent, MetaEvent, SysExEvent, RunningStatus };
    static EventType DecodeEventType( int iEventCode );

    //Parsing functions that load data into the instance
    static int MakeNextEvent( const unsigned char *pcData, int iMaxSize, int iTrack, MIDIEvent **pOutEvent );

    //Accessors
    EventType GetEventType() const { return m_eEventType; }
    int GetEventCode() const { return m_iEventCode; }
    int GetTrack() const { return m_iTrack; }
    int GetAbsT() const { return m_iAbsT; }
    long long GetAbsMicroSec() const { return m_llAbsMicroSec; }
    void SetAbsMicroSec( long long llAbsMicroSec ) { m_llAbsMicroSec = llAbsMicroSec; }

protected:
    EventType m_eEventType;
    unsigned char m_iEventCode;
    unsigned short m_iTrack;
    int m_iAbsT;
    long long m_llAbsMicroSec;
};

//Channel Event: notes and whatnot
class MIDIChannelEvent : public MIDIEvent
{
public:
    MIDIChannelEvent() : m_pSister( NULL ), m_iSimultaneous( 0 ) { }

    enum ChannelEventType : unsigned char { NoteOff = 0x8, NoteOn, NoteAftertouch, Controller, ProgramChange, ChannelAftertouch, PitchBend };
    int ParseEvent( const unsigned char *pcData, int iMaxSize );

    //Accessors
    ChannelEventType GetChannelEventType() const { return m_eChannelEventType; }
    unsigned char GetChannel() const { return m_cChannel; }
    unsigned char GetParam1() const { return m_cParam1; }
    unsigned char GetParam2() const { return m_cParam2; }
    MIDIChannelEvent *GetSister() const { return m_pSister; }
    int GetSimultaneous() const { return m_iSimultaneous; }

    void SetSister( MIDIChannelEvent *pSister ) { m_pSister = pSister; pSister->m_pSister = this; }
    void SetSimultaneous( int iSimultaneous ) { m_iSimultaneous = iSimultaneous; }

private:
	MIDIChannelEvent *m_pSister;
    ChannelEventType m_eChannelEventType;
    unsigned char m_cChannel;
    unsigned char m_cParam1;
    unsigned char m_cParam2;
    int m_iSimultaneous;
};

//Meta Event: info about the notes and whatnot
class MIDIMetaEvent : public MIDIEvent
{
public:
    MIDIMetaEvent() : m_pcData( 0 ) { }
    ~MIDIMetaEvent() { if ( m_pcData ) delete[] m_pcData; }

    enum MetaEventType { SequenceNumber, TextEvent, Copyright, SequenceName, InstrumentName, Lyric, Marker,
                         CuePoint, ChannelPrefix = 0x20, PortPrefix = 0x21, EndOfTrack = 0x2F, SetTempo = 0x51,
                         SMPTEOffset = 0x54, TimeSignature = 0x58, KeySignature = 0x59, Proprietary = 0x7F,
						 SysEx = 0xF0 };
    int ParseEvent( const unsigned char *pcData, int iMaxSize );

    //Accessors
    MetaEventType GetMetaEventType() const { return m_eMetaEventType; }
    int GetDataLen() const { return m_iDataLen; }
    unsigned char *GetData() const { return m_pcData; }

protected:
    MetaEventType m_eMetaEventType;
    int m_iDataLen;
    unsigned char *m_pcData;
};

//SysEx Event: probably to be ignored
class MIDISysExEvent : public MIDIMetaEvent
{
public:
	MIDISysExEvent() {}
    ~MIDISysExEvent() {}

    int ParseEvent( const unsigned char *pcData, int iMaxSize );

public:
    int m_iSysExCode;
    bool m_bHasMoreData;
    MIDISysExEvent *prevEvent;
};

//
// MIDI Device Classes
//

class MIDIDevice
{
public:
    virtual int GetNumDevs() const = 0;
    virtual wstring GetDevName( int iDev ) const = 0;
    virtual bool Open( int iDev ) = 0;
    virtual void Close() = 0;

    bool IsOpen() const { return m_bIsOpen; }
    const wstring &GetDevice() const { return m_sDevice; };

protected:
    MIDIDevice() : m_bIsOpen( false ), m_iDevice( 0 ) { }
    virtual ~MIDIDevice() { }

    bool m_bIsOpen;
    int m_iDevice;
    wstring m_sDevice;
};

extern "C"
{
	typedef MMRESULT(WINAPI*kShortMsg)(DWORD dwMsg);
	typedef MMRESULT(WINAPI*kLongMsg)(LPMIDIHDR hdr);
	typedef BOOL(WINAPI*pfnBoolFunc)(void);
}

class MIDIOutDevice : public MIDIDevice
{
public:
	static void SetKDMPath(const wstring &path);

    MIDIOutDevice() : m_hMIDIOut( NULL ), m_hmKDM(NULL), m_pfnKShortMsg(NULL), m_pfnKLongMsg(NULL) { }
    virtual ~MIDIOutDevice() { Close(); }

    int GetNumDevs() const;
    wstring GetDevName( int iDev ) const;
    bool Open( int iDev );
    void Close();

    void AllNotesOff();
    void AllNotesOff( const vector< int > &vChannels );
    void SetVolume( double dVolume );

    bool PlayEventAcrossChannels( unsigned char cStatus, unsigned char cParam1, unsigned char cParam2 );
    bool PlayEventAcrossChannels( unsigned char cStatus, unsigned char cParam1, unsigned char cParam2, const vector< int > &vChannels );
    bool PlayEvent( unsigned char bStatus, unsigned char bParam1, unsigned char bParam2 = 0 );
	bool PlayEvent(const MIDIEvent* evt);

private:
    static void CALLBACK MIDIOutProc( HMIDIOUT hmo, UINT wMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2 );
	static MMRESULT WINAPI KDMShortMsg(HMIDIOUT hmo, DWORD dwMsg);
    HMIDIOUT m_hMIDIOut;
	HMODULE m_hmKDM;
	kShortMsg m_pfnKShortMsg;
	kLongMsg m_pfnKLongMsg;
	static wstring s_KDMPath;
};

