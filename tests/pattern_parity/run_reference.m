function run_reference(guiFile, caseDir, outDir)
% Writes the MATLAB reference .bin for every test case, using the backend of
% PatternGenGUI.m itself (generateWaveformsFromUITable, getReorderForDevice)
% plus the pad / remap / fwrite steps of its onGenerateFile callback.
%
%   matlab -batch "run_reference('../../tools/matlab/PatternGenGUI.m','cases','out_matlab')"

src = readlines(guiFile);
tmp = tempname; mkdir(tmp);
cleanup = onCleanup(@() removeTempPath(tmp));

% Local functions can't be called from outside the file, so copy them into
% their own function files.
extractFunction(src, 'function [waveData, t, durationVal', '%% --------- Range parsing', ...
    fullfile(tmp,'generateWaveformsFromUITable.m'));
extractFunction(src, 'function reorder = getReorderForDevice', '%% --------- Parameter guide', ...
    fullfile(tmp,'getReorderForDevice.m'));
addpath(tmp);

if ~exist(outDir,'dir'), mkdir(outDir); end
cases = dir(fullfile(caseDir,'*.csv'));

for k = 1:numel(cases)
    [durationStr, dev, data] = readCase(fullfile(caseDir, cases(k).name));

    tbl = struct('Data', {data});
    [waveData, ~, ~] = generateWaveformsFromUITable(tbl, durationStr);

    % --- same steps as onGenerateFile ---
    numSamples = size(waveData,2);
    remainder = mod(numSamples,4);
    if remainder > 0
        waveData = [waveData, zeros(64,4 - remainder,'uint8')]; %#ok<AGROW>
    end
    reorder = getReorderForDevice(dev);
    remapped = zeros(size(waveData),'uint8');
    for i=1:64
        remapped(i,:) = waveData(reorder(i),:);
    end
    % ------------------------------------

    [~, name] = fileparts(cases(k).name);
    fid = fopen(fullfile(outDir,[name '.bin']),'wb');
    fwrite(fid, remapped, 'uint8');
    fclose(fid);
end

fprintf('MATLAB: wrote %d pattern files to %s\n', numel(cases), outDir);
end

function removeTempPath(tmp)
rmpath(tmp);
rmdir(tmp,'s');
end

function extractFunction(src, startText, endText, outFile)
first = find(startsWith(src, startText), 1);
last = find(startsWith(src, endText), 1) - 1;
assert(~isempty(first) && ~isempty(last) && last > first, ...
    'Could not find %s in PatternGenGUI.m', startText);
writelines(src(first:last), outFile);
end

function [durationStr, dev, data] = readCase(file)
lines = readlines(file);
lines = lines(strlength(strtrim(lines)) > 0);
durationStr = '';
dev = '';
data = cell(0,8);
for i = 1:numel(lines)
    f = split(lines(i), ',');
    switch f(1)
        case "duration"
            durationStr = char(f(2));
        case "device"
            dev = char(f(2));
        otherwise
            row = {str2double(f(1)), char(f(2)), str2double(f(3)), str2double(f(4)), ...
                   str2double(f(5)), str2double(f(6)), str2double(f(7)), str2double(f(8))};
            data(end+1,:) = row; %#ok<AGROW>
    end
end
end
