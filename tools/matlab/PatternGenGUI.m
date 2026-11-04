%% 
function PatternGenGUI()
% PatternGenGUI_Resizable_uifigure
% R2019b+ responsive GUI (uifigure + uigridlayout).
%
% Sampling
% - fs = 20000 Hz, values clipped to [0..100] then uint8.
%
% fixedpulse:
%   Freq/#Pulses  = number of pulses (integer)
%   Amplitude     = ON level (baseline 0)
%   Offset/Period = period (sec)
%   Duty          = duty cycle (%) within each period
%   Delay         = start time (sec)
%
% fixedsawtooth:
%   Freq/#Pulses  = nCycles (integer)
%   Amplitude     = peak ramp amplitude A (baseline 0)
%   Offset/Period = period T (sec)
%   Duty          = ramp fraction of each period (%). rampTime=duty/100*T.
%                  If duty<100, ramp ends early but still reaches A.
%   Delay         = start time (sec)
%
% fixedsine:
%   Freq/#Pulses  = nCycles (integer)
%   Amplitude     = amplitude A (peak)
%   Offset/Period = period T (sec)  (f = 1/T)
%   Duty          = fraction (%) of each cycle that is "active".
%                  If duty<100, sine is time-compressed into duty*T but still
%                  completes a full cycle; outside active region output is 0.
%   Delay         = start time (sec)
%
% wavelet (positive-only, mirrored, fits exactly into duration):
%   Freq/#Pulses  = nPeaks (odd: 1,3,5,...)  [# positive bumps]
%   Amplitude     = amplitude (baseline 0)
%   Offset/Period = duration D (sec)
%   Duty          = unused
%   Delay         = start time (sec)
%
% pulse_staircase:
%   Freq/#Pulses  = number of pulses (integer)
%   Amplitude     = start amplitude A0
%   Step          = amplitude step per pulse (can be + or -)
%   Offset/Period = period (sec)
%   Duty          = duty cycle (%) within each period
%   Delay         = start time (sec)

%% Figure + main layout
fig = uifigure('Name','PatternGenGUI','Position',[100 100 1450 840]);

glMain = uigridlayout(fig,[1 2]);
glMain.ColumnWidth = {460,'1x'};
glMain.RowHeight   = {'1x'};
glMain.Padding = [10 10 10 10];
glMain.ColumnSpacing = 12;

%% LEFT: channel selection
glLeft = uigridlayout(glMain,[4 1]);
glLeft.RowHeight = {48, 40, 30, '1x'};
glLeft.Padding = [0 0 0 0];
glLeft.RowSpacing = 8;

% Row 1: Select/Deselect
glSel = uigridlayout(glLeft,[1 2]);
glSel.ColumnWidth = {'1x','1x'};
uibutton(glSel,'Text','Select All','ButtonPushedFcn',@onSelectAll,...
    'FontSize',14,'FontWeight','bold');
uibutton(glSel,'Text','Deselect All','ButtonPushedFcn',@onDeselectAll,...
    'FontSize',14,'FontWeight','bold');

% Row 2: Range input (default 0)
glRange = uigridlayout(glLeft,[1 3]);
glRange.ColumnWidth = {90,'1x',120};
uilabel(glRange,'Text','Range Input:','HorizontalAlignment','right');
editRange = uieditfield(glRange,'text','Value','0');
uibutton(glRange,'Text','Apply Range','ButtonPushedFcn',@onApplyRange);

% Row 3: left status
lblLeftStatus = uilabel(glLeft,'Text','Toggle channels (1..64)','FontAngle','italic');

% Row 4: toggle grid (4 shanks)
glToggles = uigridlayout(glLeft,[1 4]);
glToggles.ColumnWidth = {'1x','1x','1x','1x'};
glToggles.RowHeight = {'1x'};
glToggles.ColumnSpacing = 6;
glToggles.RowSpacing = 6;

channelToggles = gobjects(64,1);

for col = 1:4
    p = uipanel(glToggles,'Title',sprintf('SHANK %d',col));
    glShank = uigridlayout(p,[16 1]);
    glShank.RowHeight = repmat({28},1,16);
    glShank.Padding = [6 6 6 6];

    topChannel = 16*col;           % 16,32,48,64
    for row=0:15
        chNum = topChannel - row;  % descending
        channelToggles(chNum) = uibutton(glShank,'state', ...
            'Text',sprintf('CH%d',chNum), ...
            'Value',false);
    end
end

%% RIGHT: table + actions + bulk + guide
glRight = uigridlayout(glMain,[6 1]);
glRight.RowHeight = {48, '1x', 28, 44, 240, 180};
glRight.Padding = [0 0 0 0];
glRight.RowSpacing = 8;

% Row 1: Duration + Load
glTop = uigridlayout(glRight,[1 6]);
glTop.ColumnWidth = {110, 110, 10, '1x', 10, 150};

uilabel(glTop,'Text','Duration (sec):','HorizontalAlignment','right');
editDuration = uieditfield(glTop,'text','Value','1');
uilabel(glTop,'Text',''); % spacer
lblStatus = uilabel(glTop,'Text','Status: Ready','FontWeight','bold');
uilabel(glTop,'Text',''); % spacer
uibutton(glTop,'Text','Load Channels','ButtonPushedFcn',@onLoadChannels,...
    'FontSize',14,'FontWeight','bold');

% Row 2: waveform table
colNames = {'Channel','WaveType','Freq/#Pulses','Amplitude','Offset/Period','Duty (%) / Cycles','Delay (s)','Step'};
waveTypes = {'none', 'pulse', 'fixedpulse', 'pulse_staircase', 'sawtooth', 'fixedsawtooth', 'sine', 'fixedsine', 'doublesine', 'wavelet'};

tbl = uitable(glRight, ...
    'ColumnName', colNames, ...
    'ColumnEditable', [false true true true true true true true], ...
    'ColumnFormat', {[], waveTypes, 'numeric','numeric','numeric','numeric','numeric','numeric'}, ...
    'Data', cell(0,8));

tbl.SelectionChangedFcn = @onTableSelectionChanged;
tbl.CellEditCallback = @onTableCellEdited;

% Row 3: hint
uilabel(glRight,'Text','Tip: select a row to see parameter meanings below.','FontAngle','italic');

% Row 4: Plot/Generate/Clear
glBtns = uigridlayout(glRight,[1 3]);
glBtns.ColumnWidth = {'1x','1x','1x'};
uibutton(glBtns,'Text','Plot Waveforms','ButtonPushedFcn',@onPlotWaveforms);
uibutton(glBtns,'Text','Generate File','ButtonPushedFcn',@onGenerateFile);
uibutton(glBtns,'Text','Clear Table','ButtonPushedFcn',@onClearTable);

% Row 5: Bulk assignment panel (ALL defaults 0)
pBulk = uipanel(glRight,'Title','Bulk Update');
glBulk = uigridlayout(pBulk,[6 4]);
glBulk.ColumnWidth = {130,'1x',130,'1x'};
glBulk.RowHeight = {28,28,28,28,28,28};
glBulk.Padding = [8 8 8 8];
glBulk.RowSpacing = 6;
glBulk.ColumnSpacing = 10;

uilabel(glBulk,'Text','Range:','HorizontalAlignment','right');
editBulkRange = uieditfield(glBulk,'text','Value','0');

uilabel(glBulk,'Text','WaveType:','HorizontalAlignment','right');
ddBulkType = uidropdown(glBulk,'Items',waveTypes,'Value','none');

uilabel(glBulk,'Text','Freq/#Pulses:','HorizontalAlignment','right');
editBulkFreq = uieditfield(glBulk,'numeric','Value',0);

uilabel(glBulk,'Text','Amplitude:','HorizontalAlignment','right');
editBulkAmp = uieditfield(glBulk,'numeric','Value',0);

uilabel(glBulk,'Text','Offset/Period:','HorizontalAlignment','right');
editBulkOffset = uieditfield(glBulk,'numeric','Value',0);

uilabel(glBulk,'Text','Duty/Cycles:','HorizontalAlignment','right');
editBulkDuty = uieditfield(glBulk,'numeric','Value',0);

uilabel(glBulk,'Text','Delay start:','HorizontalAlignment','right');
editBulkDelay = uieditfield(glBulk,'numeric','Value',0);

uilabel(glBulk,'Text','Step:','HorizontalAlignment','right');
editBulkStep = uieditfield(glBulk,'numeric','Value',0);

btnApplyBulk = uibutton(glBulk,'Text','Apply Params','ButtonPushedFcn',@onApplyWaveformRange);
btnApplyBulk.Layout.Row = 5;
btnApplyBulk.Layout.Column = [3 4];

uilabel(glBulk,'Text','Delay step Δt:','HorizontalAlignment','right');
editBulkDelayStep = uieditfield(glBulk,'numeric','Value',0);

btnApplyDelayRamp = uibutton(glBulk,'Text','Apply Delay Ramp','ButtonPushedFcn',@onApplyDelayRamp);
btnApplyDelayRamp.Layout.Row = 6;
btnApplyDelayRamp.Layout.Column = [3 4];

% Row 6: Parameter guide panel
pGuide = uipanel(glRight,'Title','Parameter Guide (selected row)');
glGuide = uigridlayout(pGuide,[1 1]);
glGuide.Padding = [8 8 8 8];
guideTbl = uitable(glGuide, ...
    'ColumnName', {'Field','Meaning'}, ...
    'ColumnEditable', [false false], ...
    'Data', paramGuideData('none'));
guideTbl.ColumnWidth = {185,'1x'};

selectedRow = [];

%% Callbacks
    function onSelectAll(~,~)
        for c=1:64, channelToggles(c).Value = true; end
        lblLeftStatus.Text = 'All channels selected.';
    end

    function onDeselectAll(~,~)
        for c=1:64, channelToggles(c).Value = false; end
        lblLeftStatus.Text = 'All channels deselected.';
    end

    function onApplyRange(~,~)
        chList = parseRangeString(editRange.Value);
        if any(chList < 1 | chList > 64)
            lblStatus.Text = 'Status: Error (range must be within 1..64)';
            return;
        end
        for c = chList
            channelToggles(c).Value = ~channelToggles(c).Value;
        end
        lblLeftStatus.Text = sprintf('Toggled %d channel(s).', numel(chList));
    end

    function onLoadChannels(~,~)
        answer = uiconfirm(fig, ...
            'This will overwrite the current table of loaded channels. Continue?', ...
            'Load Channels?', ...
            'Options',{'Yes','No'}, 'DefaultOption',2, 'CancelOption',2);
        if ~strcmp(answer,'Yes'), return; end

        selCh = find(arrayfun(@(h) h.Value, channelToggles));
        if isempty(selCh)
            lblStatus.Text = 'Status: Error (no channels selected)';
            return;
        end

        newData = cell(numel(selCh),8);
        for i=1:numel(selCh)
            newData{i,1} = selCh(i);
            newData{i,2} = 'none';
            newData{i,3} = 0;
            newData{i,4} = 0;
            newData{i,5} = 0;
            newData{i,6} = 0;
            newData{i,7} = 0;
            newData{i,8} = 0;
        end
        tbl.Data = newData;
        selectedRow = 1;
        guideTbl.Data = paramGuideData('none');
        lblStatus.Text = sprintf('Status: Loaded %d channel(s).', numel(selCh));
    end

    function onTableSelectionChanged(~,evt)
        if isempty(evt.Selection)
            selectedRow = [];
            guideTbl.Data = paramGuideData('none');
            return;
        end
        selectedRow = evt.Selection(1);
        wType = tbl.Data{selectedRow,2};
        if isempty(wType), wType = 'none'; end
        guideTbl.Data = paramGuideData(wType);
    end

    function onTableCellEdited(~,evt)
        if isempty(selectedRow)
            if ~isempty(evt.Indices), selectedRow = evt.Indices(1); end
        end
        if isempty(selectedRow), return; end
        wType = tbl.Data{selectedRow,2};
        if isempty(wType), wType = 'none'; end
        guideTbl.Data = paramGuideData(wType);
    end

    function onApplyWaveformRange(~,~)
        rCh = parseRangeString(editBulkRange.Value);
        if isempty(rCh)
            lblStatus.Text = 'Status: Error (no channels in bulk range)';
            return;
        end

        freqVal  = editBulkFreq.Value;
        ampVal   = editBulkAmp.Value;
        offVal   = editBulkOffset.Value;
        dutyVal  = editBulkDuty.Value;
        delayVal = editBulkDelay.Value;
        stepVal  = editBulkStep.Value;
        wType    = ddBulkType.Value;

        if any(~isfinite([freqVal ampVal offVal dutyVal delayVal stepVal]))
            lblStatus.Text = 'Status: Error (invalid bulk values)';
            return;
        end
        ampVal = min(max(ampVal,0),100);

        data = tbl.Data;
        if isempty(data)
            lblStatus.Text = 'Status: Error (no channels loaded)';
            return;
        end

        updated = 0;
        for r=1:size(data,1)
            ch = data{r,1};
            if ismember(ch, rCh)
                data{r,2} = wType;
                data{r,3} = freqVal;
                data{r,4} = ampVal;
                data{r,5} = offVal;
                data{r,6} = dutyVal;
                data{r,7} = delayVal;
                data{r,8} = stepVal;
                updated = updated + 1;
            end
        end
        tbl.Data = data;
        guideTbl.Data = paramGuideData(wType);
        lblStatus.Text = sprintf('Status: Bulk updated %d channel(s).', updated);
    end

    function onApplyDelayRamp(~,~)
        rCh = parseRangeString(editBulkRange.Value);
        if isempty(rCh)
            lblStatus.Text = 'Status: Error (no channels in delay-ramp range)';
            return;
        end

        startDelay = editBulkDelay.Value;
        dt = editBulkDelayStep.Value;

        if any(~isfinite([startDelay dt])) || dt < 0
            lblStatus.Text = 'Status: Error (invalid start delay or Δt)';
            return;
        end

        data = tbl.Data;
        if isempty(data)
            lblStatus.Text = 'Status: Error (no channels loaded)';
            return;
        end

        chCol = 1; delayCol = 7;
        rows = find(ismember(cell2mat(data(:,chCol)), rCh));
        if isempty(rows)
            lblStatus.Text = 'Status: Error (no matching channels in table)';
            return;
        end

        chNums = cell2mat(data(rows, chCol));
        [~, ord] = sort(chNums);
        rows = rows(ord);

        for k = 1:numel(rows)
            data{rows(k), delayCol} = startDelay + (k-1)*dt;
        end

        tbl.Data = data;
        lblStatus.Text = sprintf('Status: Applied delay ramp to %d channel(s).', numel(rows));
    end

    function onClearTable(~,~)
        answer = uiconfirm(fig,'Clear the entire waveform table?','Clear Table?', ...
            'Options',{'Yes','No'}, 'DefaultOption',2, 'CancelOption',2);
        if ~strcmp(answer,'Yes'), return; end
        tbl.Data = cell(0,8);
        selectedRow = [];
        guideTbl.Data = paramGuideData('none');
        lblStatus.Text = 'Status: Table cleared.';
    end

    function onPlotWaveforms(~,~)
        try
            [waveData, t, durationVal, loadedChannels, channelConfigs] = ...
                generateWaveformsFromUITable(tbl, editDuration.Value);

            nLoaded = numel(loadedChannels);
            if nLoaded < 1, error('No channels to plot.'); end

            figure('Name','Loaded Channels (0..100)','NumberTitle','off');
            nRows = ceil(sqrt(nLoaded));
            nCols = ceil(nLoaded/nRows);

            for i=1:nLoaded
                ch = loadedChannels(i);
                subplot(nRows,nCols,i);
                plot(t, double(waveData(ch,:)));
                title(sprintf('CH %d: %s', ch, channelConfigs(ch).type));
                xlim([0 durationVal]); ylim([0 100]);
            end
            lblStatus.Text = 'Status: Plotted waveforms.';
        catch ME
            lblStatus.Text = ['Status: Error (', ME.message, ')'];
        end
    end

    function onGenerateFile(~,~)
        try
            [waveData, ~, ~] = generateWaveformsFromUITable(tbl, editDuration.Value);

            numSamples = size(waveData,2);
            remainder = mod(numSamples,4);
            if remainder > 0
                padNeeded = 4 - remainder;
                waveData = [waveData, zeros(64,padNeeded,'uint8')];
            else
                padNeeded = 0;
            end
            numSamplesPadded = size(waveData,2);

            % -------- NEW: Prompt for target device and choose reorder mapping
           dev = uiconfirm(fig, ...
                'Select the target device for channel remapping:', ...
                'Target Device', ...
                'Options', {'4 Shank Chronic','2 Shank Chronic','4 Shank Acute','Demo Board'}, ...
                'DefaultOption', 1, ...
                'CancelOption', 1);   % user can still close the dialog (treat as cancel below)
            
            if isempty(dev) || ~ischar(dev)
                lblStatus.Text = 'Status: Save canceled (no device selected).';
                return;
            end

            reorder = getReorderForDevice(dev);  % 1x64, values 1..64

            remapped = zeros(size(waveData),'uint8');
            for i=1:64
                remapped(i,:) = waveData(reorder(i),:);
            end
            % -------- END NEW

            [file,path] = uiputfile({'*.bin','Binary Files (*.bin)'},'Save As','DACData.bin');
            if isequal(file,0)
                lblStatus.Text = 'Status: Save canceled.';
                return;
            end
            fid = fopen(fullfile(path,file),'wb');
            if fid < 0, error('Cannot open output file for writing.'); end
            count = fwrite(fid, remapped, 'uint8');
            fclose(fid);

            padMsg = '';
            if padNeeded>0, padMsg = sprintf(' (Padded %d)',padNeeded); end
            lblStatus.Text = sprintf('Status: Wrote %d bytes (%d samples)%s [%s]', ...
                count, numSamplesPadded, padMsg, dev);
        catch ME
            lblStatus.Text = ['Status: Error (', ME.message, ')'];
        end
    end

end % end main

%% --------- Waveform generation (backend)
function [waveData, t, durationVal, loadedChannels, channelConfigs] = generateWaveformsFromUITable(tbl, durationIn)

data = tbl.Data;
if isempty(data)
    error('No channel data in table. Please load channels first.');
end

durationVal = str2double(string(durationIn));
if isnan(durationVal) || durationVal <= 0
    error('Invalid duration; must be positive.');
end

fs = 20000;
numSamples = round(durationVal * fs);
t = (0:numSamples-1)/fs;

loadedChannels = cell2mat(data(:,1));

channelConfigs = repmat(struct('type','none','freq',0,'amp',0,'offset',0,'duty',0,'delay',0,'step',0),1,64);
for r=1:size(data,1)
    ch = data{r,1};
    channelConfigs(ch).type   = data{r,2};
    channelConfigs(ch).freq   = data{r,3};
    channelConfigs(ch).amp    = min(data{r,4},100);
    channelConfigs(ch).offset = data{r,5};
    channelConfigs(ch).duty   = data{r,6};
    channelConfigs(ch).delay  = data{r,7};
    channelConfigs(ch).step   = data{r,8};
end

waveDataDouble = zeros(64,numSamples);

for ch=1:64
    cfg = channelConfigs(ch);
    y = zeros(1,numSamples);

    if ~strcmp(cfg.type,'none')
        idx = (t >= cfg.delay);
        td  = t(idx) - cfg.delay;

        switch cfg.type
            case 'sine'
                y(idx) = cfg.offset + cfg.amp .* sin(2*pi*cfg.freq .* td);

            case 'fixedsine'
                nCycles = round(cfg.freq);
                T = cfg.offset;        % period (sec)
                duty = cfg.duty;       % active fraction per cycle (%)
                tStart = cfg.delay;
                A = cfg.amp;

                y(:) = 0;
                if nCycles > 0 && isfinite(T) && T > 0
                    duty = max(0, min(100, duty));
                    activeT = (duty/100) * T;

                    tEnd = tStart + nCycles*T;
                    in = (t >= tStart) & (t < tEnd);
                    tau = t(in) - tStart;
                    phase = mod(tau, T); % 0..T

                    yLocal = zeros(size(phase)); % baseline 0
                    if activeT > 0
                        active = phase < activeT;
                        phi = (phase(active) / activeT) * (2*pi);
                        yLocal(active) = A * sin(phi);
                    end

                    y(in) = yLocal;
                end

            case 'pulse'
                y(idx) = cfg.offset + cfg.amp .* square(2*pi*cfg.freq .* td, cfg.duty);

            case 'sawtooth'
                y(idx) = cfg.offset + cfg.amp .* sawtooth(2*pi*cfg.freq .* td);

            case 'fixedsawtooth'
                nCycles = round(cfg.freq);
                T = cfg.offset;        % period (sec)
                duty = cfg.duty;       % ramp % within period
                tStart = cfg.delay;
                A = cfg.amp;

                y(:) = 0;
                if nCycles > 0 && isfinite(T) && T > 0
                    duty = max(0, min(100, duty));
                    rampT = (duty/100) * T;

                    tEnd = tStart + nCycles*T;
                    in = (t >= tStart) & (t < tEnd);
                    tau = t(in) - tStart;

                    phase = mod(tau, T); % 0..T

                    yLocal = zeros(size(phase)); % baseline 0
                    if rampT > 0
                        ramping = phase < rampT;
                        yLocal(ramping) = A * (phase(ramping) / rampT);
                    end
                    y(in) = yLocal;
                end

            case 'doublesine'
                f1 = cfg.freq;
                f2 = cfg.offset;  % repurposed
                base = cfg.duty;  % repurposed
                y(idx) = base + (cfg.amp/2) * (sin(2*pi*f1.*td) + sin(2*pi*f2.*td));

            case 'fixedpulse'
                nPulses = round(cfg.freq);
                period  = cfg.offset; % period (seconds)
                wavePart = zeros(size(td));

                if (nPulses > 0) && (period > 0)
                    totalTime = nPulses * period;
                    inTrain = (td < totalTime);

                    td2 = td(inTrain);
                    localFrac = mod(td2, period) / period;
                    isHigh = (localFrac < (cfg.duty/100));

                    waveLocal = zeros(size(td2));
                    waveLocal(isHigh) = cfg.amp;
                    wavePart(inTrain) = waveLocal;
                end
                y(idx) = wavePart;

            case 'wavelet'
                nPeaks = round(cfg.freq);
                D      = cfg.offset;
                A      = cfg.amp;
                tStart = cfg.delay;

                if ~isfinite(D) || D <= 0 || ~isfinite(nPeaks) || nPeaks < 1
                    y(:) = 0;
                else
                    if mod(nPeaks,2) == 0
                        nPeaks = nPeaks + 1;
                    end

                    y(:) = 0;
                    tEnd = tStart + D;
                    in = (t >= tStart) & (t <= tEnd);

                    tc = tStart + D/2;
                    u  = (t(in) - tc) / (D/2);

                    w = 0.5 * (1 + cos(pi*u));
                    cycles = (nPeaks - 1)/2;
                    f = cycles / D;

                    carrier = cos(2*pi*f*(t(in) - tc)).^2;
                    y(in) = A .* w .* carrier;
                end

            case 'pulse_staircase'
                nPulses = round(cfg.freq);
                A0      = cfg.amp;
                dA      = cfg.step;
                period  = cfg.offset;
                duty    = cfg.duty;
                tStart  = cfg.delay;

                y(:) = 0;
                if (nPulses > 0) && (period > 0)
                    tEnd = tStart + nPulses*period;
                    in = (t >= tStart) & (t < tEnd);
                    tau = t(in) - tStart;

                    pulseIdx = floor(tau/period);
                    localFrac = mod(tau, period)/period;
                    isHigh = localFrac < (duty/100);

                    ampPerSample = A0 + dA .* pulseIdx;
                    yLocal = zeros(size(tau));
                    yLocal(isHigh) = ampPerSample(isHigh);

                    y(in) = yLocal;
                end

            otherwise
        end
    end

    y(y<0)=0; y(y>100)=100;
    waveDataDouble(ch,:) = y;
end

waveData = uint8(round(waveDataDouble));
end

%% --------- Range parsing
function chList = parseRangeString(strRng)
chList = [];
if isempty(strRng), return; end
parts = strsplit(strtrim(strRng),',');
for i=1:numel(parts)
    token = strtrim(parts{i});
    dashPos = strfind(token,'-');
    if isempty(dashPos)
        v = str2double(token);
        if ~isnan(v), chList(end+1) = v; end %#ok<AGROW>
    else
        a = str2double(token(1:dashPos(1)-1));
        b = str2double(token(dashPos(1)+1:end));
        if ~isnan(a) && ~isnan(b) && b>=a
            chList = [chList, a:b]; %#ok<AGROW>
        end
    end
end
chList = unique(chList);
end

function reorder = getReorderForDevice(dev)
% dev is a char vector in R2019b

switch dev
    case '4 Shank Chronic'
        reorder = [17 34 19 36 21 38 23 40 25 42 27 44 29 46 31 48 ...
                   32 47 30 45 28 43 26 41 24 39 22 37 20 35 18 33 ...
                   1 50 3 52 5 54 7 56 9 58 11 60 13 62 15 64 ...
                   16 63 14 61 12 59 10 57 8 55 6 53 4 51 2 49];

    case '2 Shank Chronic'
        reorder = [1 34 3 36 5 38 7 40 9 42 11 44 13 46 15 48 ...
                   17 50 19 52 21 54 23 56 25 58 27 60 29 62 31 64 ...
                   32 63 30 61 28 59 26 57 24 55 22 53 20 51 18 49 ...
                   16 47 14 45 12 43 10 41 8 39 6 37 4 35 2 33];
                    
    case '4 Shank Acute'
        reorder = [33 18 35 20 37 22 39 24 41 26 43 28 45 30 47 32 ...
                   48 31 46 29 44 27 42 25 40 23 38 21 36 19 34 17 ...
                   50 1  52 3  54 5  56 7  58 9  60 11 62 13 64 15 ...
                   63 16 61 14 59 12 57 10 55 8  53 6  51  4 49 2];
                   

    case 'Demo Board'
        reorder = [16 14 12 10 8 6 4 2 64 62 60 58 56 54 52 50 ...
                   32 30 28 26 24 22 20 18 48 46 44 42 40 38 36 34 ...
                   15 13 11 9 7 5 3 1 63 61 59 57 55 53 51 49 ...
                   31 29 27 25 23 21 19 17 47 45 43 41 39 37 35 33];

    otherwise
        error('Unknown target device: %s', dev);
end

if numel(reorder) ~= 64 || ~isequal(sort(reorder(:))', 1:64)
    error('Invalid reorder mapping for %s. Must be a permutation of 1:64.', dev);
end
end

%% --------- Parameter guide "dictionary"
function guide = paramGuideData(wType)
wType = lower(string(wType));
switch wType
    case "none"
        guide = {'WaveType','none (channel stays at 0)'
            'Freq/#Pulses','unused'
            'Amplitude','unused'
            'Offset/Period','unused'
            'Duty/Cycles','unused'
            'Delay (s)','unused'
            'Step','unused'};

    case "sine"
        guide = {'WaveType','Sine wave'
            'Freq/#Pulses','Frequency (Hz)'
            'Amplitude','Amplitude'
            'Offset/Period','Offset (baseline)'
            'Duty/Cycles','unused'
            'Delay (s)','Start delay (s)'
            'Step','unused'};

    case "fixedsine"
        guide = {'WaveType','Fixed-count sine (duty-limited active segment)'
            'Freq/#Pulses','nCycles (integer)'
            'Amplitude','Amplitude A'
            'Offset/Period','Period T (seconds)'
            'Duty/Cycles','Active fraction of T (%); sine is time-compressed to fit'
            'Delay (s)','Start time (s)'
            'Step','unused'};

    case "pulse"
        guide = {'WaveType','Square wave'
            'Freq/#Pulses','Frequency (Hz)'
            'Amplitude','Amplitude (added on top of offset)'
            'Offset/Period','Offset (baseline)'
            'Duty/Cycles','Duty cycle (%)'
            'Delay (s)','Start delay (s)'
            'Step','unused'};

    case "sawtooth"
        guide = {'WaveType','Sawtooth wave'
            'Freq/#Pulses','Frequency (Hz)'
            'Amplitude','Amplitude'
            'Offset/Period','Offset (baseline)'
            'Duty/Cycles','unused'
            'Delay (s)','Start delay (s)'
            'Step','unused'};

    case "fixedsawtooth"
        guide = {'WaveType','Fixed-count sawtooth (duty-limited ramp)'
            'Freq/#Pulses','nCycles (integer)'
            'Amplitude','Peak ramp amplitude A (baseline 0)'
            'Offset/Period','Period T (seconds)'
            'Duty/Cycles','Ramp fraction of T (%); rampTime = duty/100*T'
            'Delay (s)','Start time (s)'
            'Step','unused'};

    case "doublesine"
        guide = {'WaveType','Sum of two sines'
            'Freq/#Pulses','Frequency 1 (Hz)'
            'Amplitude','Overall amplitude (split across both sines)'
            'Offset/Period','Frequency 2 (Hz) [repurposed]'
            'Duty/Cycles','Baseline offset [repurposed]'
            'Delay (s)','Start delay (s)'
            'Step','unused'};

    case "fixedpulse"
        guide = {'WaveType','Fixed pulse train then 0'
            'Freq/#Pulses','Number of pulses (integer)'
            'Amplitude','ON level (baseline 0)'
            'Offset/Period','Period (seconds)'
            'Duty/Cycles','Duty cycle (%)'
            'Delay (s)','Start time of train (s)'
            'Step','unused'};

    case "wavelet"
        guide = {'WaveType','Positive-only mirrored burst (window * cos^2)'
            'Freq/#Pulses','nPeaks (odd: 1,3,5,...) = # positive bumps'
            'Amplitude','Amplitude (baseline 0)'
            'Offset/Period','Duration D (seconds), forced to 0 at edges'
            'Duty/Cycles','unused'
            'Delay (s)','Start time of burst (s)'
            'Step','unused'};

    case "pulse_staircase"
        guide = {'WaveType','Pulse train with per-pulse amplitude stepping'
            'Freq/#Pulses','Number of pulses (integer)'
            'Amplitude','Start amplitude A0'
            'Offset/Period','Period (seconds)'
            'Duty/Cycles','Duty cycle (%)'
            'Delay (s)','Start time (s)'
            'Step','Amplitude step per pulse (+/-)'};

    otherwise
        guide = {'WaveType',char(wType)
            'Freq/#Pulses','see code'
            'Amplitude','see code'
            'Offset/Period','see code'
            'Duty/Cycles','see code'
            'Delay (s)','see code'
            'Step','see code'};
end
end
